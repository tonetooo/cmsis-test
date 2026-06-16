#include "tasks.h"
#include "wdt.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include "console.h"
#include "FreeRTOS.h"
#include "task.h"

extern FATFS fs;
extern FIL fil;

char latest_filename[32] = {0};
uint8_t *upload_buf = NULL;
uint32_t upload_buf_size = 0;

void StartFileTask(void *argument) {
    CONS_INFO("[FILE] Task started (prio=Normal)");
    CONS_INFO("[FILE] Waiting for sensor data on queue...");

    char filename[32];
    uint8_t file_open = 0;
    uint32_t last_write_tick = 0;
    uint32_t write_count = 0;
    uint32_t mutex_acquire_count = 0;
    uint32_t mutex_contention_count = 0;

    /* Pre-allocate binary upload buffer (28 bytes/sample, 32KB = 1170 samples).
     * Each sample is accumulated DURING writing to SD — no f_lseek needed.
     * Backend receives binary and converts to CSV. */
    uint32_t bin_offset = 0;  /* Current write position in binary buffer */
    uint32_t bin_capacity = UPLOAD_BUF_CAPACITY;

    /* Don't allocate yet — wait for first sample to confirm heap availability */

    for (;;) {
        WDT_Refresh();
        SensorReading_t reading;
        osStatus_t status = osMessageQueueGet(sensor_queueHandle, &reading, NULL, 200);

        if (status == osOK) {
            /* === ACQUIRE SD MUTEX === */
            mutex_acquire_count++;
            if (osMutexAcquire(sd_mutexHandle, 10) != osOK) {
                mutex_contention_count++;
                CONS_WARN("[FILE][MUTEX] Wait #%lu (contention #%lu)... waiting indefinitely",
                          (unsigned long)mutex_acquire_count,
                          (unsigned long)mutex_contention_count);
                osMutexAcquire(sd_mutexHandle, osWaitForever);
            }
            /* === MUTEX ACQUIRED === */

            if (!file_open) {
                /* Free previous upload buffer if any */
                if (upload_buf != NULL) {
                    vPortFree(upload_buf);
                    upload_buf = NULL;
                    upload_buf_size = 0;
                }
                bin_offset = 0;

                /* Pre-allocate binary buffer for this acquisition */
                if (upload_buf == NULL) {
                    upload_buf = (uint8_t *)pvPortMalloc(bin_capacity);
                    if (upload_buf != NULL) {
                        bin_offset = 0;
                        CONS_OK("[FILE] Binary buffer allocated (%lu bytes, heap free~%lu)",
                                (unsigned long)bin_capacity,
                                (unsigned long)xPortGetFreeHeapSize());
                    } else {
                        CONS_ERR("[FILE] pvPortMalloc(%lu) FAILED (heap free=%lu) — upload will use SD",
                                 (unsigned long)bin_capacity,
                                 (unsigned long)xPortGetFreeHeapSize());
                    }
                }

                /* Find next available filename */
                CONS_INFO("[FILE] Scanning for next available TRIG_XXX.CSV...");
                for (int i = 1; i < 999; i++) {
                    sprintf(filename, "TRIG_%03d.CSV", i);
                    if (f_open(&fil, filename, FA_READ) != FR_OK) break;
                    f_close(&fil);
                }
                CONS_DBG("[FILE] Next filename: %s", filename);

                WDT_Refresh();
                if (f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
                    /* Write CSV header */
                    char header[] = "timestamp_rel_s;timestamp_abs;unix_time;x_g;y_g;z_g;voltaje;corriente;potencia\r\n";
                    UINT bw;
                    f_write(&fil, header, strlen(header), &bw);
                    CONS_DBG("[FILE][CSV] Header written (%u bytes)", bw);
                    f_sync(&fil);
                    file_open = 1;
                    write_count = 0;
                    CONS_OK("[FILE] Opened %s for writing", filename);
                } else {
                    CONS_ERR("[FILE] Could not create %s", filename);
                }
            }

            if (file_open) {
                /* === DUAL WRITE: CSV to SD + binary to RAM === */

                /* 1) Write CSV line to SD */
                char buf[128];
                uint32_t rel_sec = reading.timestamp_ms / 1000;
                uint32_t rel_us = (reading.timestamp_ms % 1000) * 1000;
                uint32_t abs_sec = 1767817653 + rel_sec;
                int line_len = sprintf(buf, "%lu.%03lu;%lu.%03lu;%lu.%03lu;%.6f;%.6f;%.6f;%.2f;%.2f;%.2f\r\n",
                        rel_sec, rel_us / 1000,
                        abs_sec, rel_us / 1000,
                        abs_sec, rel_us / 1000,
                        reading.x_g, reading.y_g, reading.z_g,
                        reading.voltage, reading.current, reading.power);

                UINT bw;
                f_write(&fil, buf, (UINT)line_len, &bw);

                /* 2) Accumulate binary record in RAM (28 bytes, packed) */
                if (upload_buf != NULL &&
                    (bin_offset + sizeof(BinarySample_t)) <= bin_capacity) {
                    BinarySample_t *sample = (BinarySample_t *)(upload_buf + bin_offset);
                    sample->timestamp_ms = reading.timestamp_ms;
                    sample->x_g = reading.x_g;
                    sample->y_g = reading.y_g;
                    sample->z_g = reading.z_g;
                    sample->voltage = reading.voltage;
                    sample->current = reading.current;
                    sample->power = reading.power;
                    bin_offset += sizeof(BinarySample_t);
                }

                write_count++;
                if (write_count <= 3) {
                    CONS_DBG("[FILE][CSV] Sample #%lu: %s", (unsigned long)write_count, buf);
                }

                if (write_count % 100 == 0) {
                    uint32_t sz = f_size(&fil);
                    CONS_DBG("[FILE] [SYNC] f_sync at sample #%lu (file ~%lu KB, RAM ~%lu KB)",
                             (unsigned long)write_count,
                             (unsigned long)(sz / 1024),
                             (unsigned long)(bin_offset / 1024));
                    f_sync(&fil);
                }

                last_write_tick = osKernelGetTickCount();
            }

            /* === RELEASE SD MUTEX === */
            osMutexRelease(sd_mutexHandle);
        }

        /* Check if acquisition is done */
        uint32_t flags = osEventFlagsGet(sensor_event_flagsHandle);
        if (file_open && (flags & EVT_ACQSTN_DONE)) {
            if (osMessageQueueGetCount(sensor_queueHandle) == 0) {
                uint32_t now = osKernelGetTickCount();
                if ((now - last_write_tick) > 500) {
                    /* === ACQUIRE SD MUTEX (close) === */
                    CONS_INFO("[FILE] Acquisition done, finalizing (samples=%lu, CSV=%lu bytes, BIN=%lu bytes)...",
                              (unsigned long)write_count,
                              (unsigned long)f_size(&fil),
                              (unsigned long)bin_offset);
                    osMutexAcquire(sd_mutexHandle, osWaitForever);

                    f_sync(&fil);
                    f_close(&fil);

                    osMutexRelease(sd_mutexHandle);
                    /* === MUTEX RELEASED === */

                    /* Finalize upload buffer */
                    if (upload_buf != NULL && bin_offset > 0) {
                        upload_buf_size = bin_offset;
                        CONS_OK("[FILE] Binary buffer ready: %lu bytes (%lu samples × %lu)",
                                (unsigned long)upload_buf_size,
                                (unsigned long)write_count,
                                (unsigned long)sizeof(BinarySample_t));
                    } else {
                        CONS_WARN("[FILE] No binary buffer — upload will try SD (may fail after modem brown-out)");
                    }

                    /* Use .BIN extension for upload (binary format) */
                    strncpy(latest_filename, filename, sizeof(latest_filename) - 1);
                    latest_filename[sizeof(latest_filename) - 1] = '\0';
                    /* Replace .CSV with .BIN for upload */
                    char *dot = strrchr(latest_filename, '.');
                    if (dot) strcpy(dot, ".BIN");

                    osEventFlagsSet(sensor_event_flagsHandle, EVT_FILE_READY);
                    CONS_OK("[FILE] EVT_FILE_READY signaled, modem_task will upload '%s'", latest_filename);

                    /* Clear ACQSTN_DONE so sensor_task knows file_task has finished */
                    osEventFlagsClear(sensor_event_flagsHandle, EVT_ACQSTN_DONE);

                    file_open = 0;
                }
            }
        }

        osDelay(1);
    }
}
