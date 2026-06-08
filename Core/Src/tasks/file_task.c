#include "tasks.h"
#include "ff.h"
#include "algo/csv_algo.h"
#include <stdio.h>
#include <string.h>

extern FATFS fs;
extern FIL fil;

char latest_filename[32] = {0};

void StartFileTask(void *argument) {
    printf("[FILE] START prio=NORMAL\r\n");
    printf("[FILE] STDBY\r\n");

    char filename[32];
    uint8_t file_open = 0;
    uint32_t last_write_tick = 0;
    uint32_t write_count = 0;
    uint32_t mutex_acquire_count = 0;
    uint32_t mutex_contention_count = 0;

    #define FILE_BUFFER_SIZE 10
    SensorReading_t file_buffer[FILE_BUFFER_SIZE];
    uint8_t buffer_count = 0;

    for (;;) {
        SensorReading_t reading;
        osStatus_t status = osMessageQueueGet(sensor_queueHandle, &reading, NULL, 200);

        if (status == osOK) {
            file_buffer[buffer_count++] = reading;

            if (buffer_count >= FILE_BUFFER_SIZE) {
                /* === ACQUIRE SD MUTEX === */
                mutex_acquire_count++;
                if (osMutexAcquire(sd_mutexHandle, 10) != osOK) {
                    mutex_contention_count++;
                    printf("[FILE][MUTEX] WAIT #%lu contention=%lu\r\n",
                           (unsigned long)mutex_acquire_count,
                           (unsigned long)mutex_contention_count);
                    osMutexAcquire(sd_mutexHandle, osWaitForever);
                }
                /* === MUTEX ACQUIRED === */

                if (!file_open) {
                    /* Find next available filename */
                    printf("[FILE] SCAN-TRIG\r\n");
                    for (int i = 1; i < 999; i++) {
                        sprintf(filename, "TRIG_%03d.CSV", i);
                        if (f_open(&fil, filename, FA_READ) != FR_OK) break;
                        f_close(&fil);
                    }
                    printf("[FILE] NEXT=%s\r\n", filename);

                    if (f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
                        /* Write CSV header */
                        char header[] = "timestamp_rel_s;timestamp_abs;unix_time;x_g;y_g;z_g;voltaje;corriente;potencia\r\n";
                        UINT bw;
                        f_write(&fil, header, strlen(header), &bw);
                        printf("[FILE][CSV] HDR=%ub file=%s\r\n", bw, header);
                        f_sync(&fil);
                        file_open = 1;
                        write_count = 0;
                        printf("[FILE] OPEN %s OK\r\n", filename);
                    } else {
                        printf("[FILE] OPEN %s FAIL\r\n", filename);
                    }
                }

                if (file_open) {
                    for (uint8_t i = 0; i < buffer_count; i++) {
                        char buf[128];
                        uint32_t rel_sec = file_buffer[i].timestamp_ms / 1000;
                        uint32_t rel_ms = file_buffer[i].timestamp_ms % 1000;
                        uint32_t abs_sec = 1767817653 + rel_sec;
                        csv_format_line(buf, sizeof(buf),
                                        rel_sec, rel_ms, abs_sec,
                                        file_buffer[i].x_g, file_buffer[i].y_g, file_buffer[i].z_g,
                                        file_buffer[i].voltage, file_buffer[i].current, file_buffer[i].power);

                        UINT bw;
                        f_write(&fil, buf, strlen(buf), &bw);

                        write_count++;
                        if (write_count <= 3) {
                            printf("[FILE][CSV] #%lu %s", (unsigned long)write_count, buf);
                        }
                    }

                    buffer_count = 0;

                    if (write_count % 10 == 0) {
                        uint32_t sz = f_size(&fil);
                        printf("[FILE] [SYNC] f_sync at sample #%lu (file size ~%lu KB)\r\n",
                               (unsigned long)write_count, (unsigned long)(sz / 1024));
                        f_sync(&fil);
                    }

                    last_write_tick = osKernelGetTickCount();
                }

                /* === RELEASE SD MUTEX === */
                osMutexRelease(sd_mutexHandle);
            }
        }

        /* Check if acquisition is done */
        uint32_t flags = osEventFlagsGet(sensor_event_flagsHandle);
        if (file_open && (flags & EVT_ACQSTN_DONE)) {
            if (osMessageQueueGetCount(sensor_queueHandle) == 0 && buffer_count == 0) {
                uint32_t now = osKernelGetTickCount();
                if ((now - last_write_tick) > 500) {
                    /* === ACQUIRE SD MUTEX (close) === */
                    printf("[FILE] Acquisition done, finalizing file (total samples: %lu)...\r\n",
                           (unsigned long)write_count);
                    osMutexAcquire(sd_mutexHandle, osWaitForever);

                    f_sync(&fil);
                    uint32_t file_bytes = f_size(&fil);
                    f_close(&fil);

                    osMutexRelease(sd_mutexHandle);
                    /* === MUTEX RELEASED === */

                    printf("[FILE] [OK] Closed %s (%lu samples, %lu bytes, %.0f bytes/sample)\r\n",
                           filename,
                           (unsigned long)write_count,
                           (unsigned long)file_bytes,
                           write_count > 0 ? (float)file_bytes / write_count : 0.0f);

                    strncpy(latest_filename, filename, sizeof(latest_filename) - 1);
                    latest_filename[sizeof(latest_filename) - 1] = '\0';

                    osEventFlagsSet(sensor_event_flagsHandle, EVT_FILE_READY);
                    printf("[FILE] [OK] EVT_FILE_READY signaled, modem_task will upload '%s'\r\n", filename);

                    file_open = 0;
                }
            } else if (buffer_count > 0 && osMessageQueueGetCount(sensor_queueHandle) == 0) {
                printf("[FILE] Flushing %u remaining buffer entries...\r\n", buffer_count);
                osMutexAcquire(sd_mutexHandle, osWaitForever);

                for (uint8_t i = 0; i < buffer_count; i++) {
                    char buf[128];
                    uint32_t rel_sec = file_buffer[i].timestamp_ms / 1000;
                    uint32_t rel_ms = file_buffer[i].timestamp_ms % 1000;
                    uint32_t abs_sec = 1767817653 + rel_sec;
                    csv_format_line(buf, sizeof(buf),
                                    rel_sec, rel_ms, abs_sec,
                                    file_buffer[i].x_g, file_buffer[i].y_g, file_buffer[i].z_g,
                                    file_buffer[i].voltage, file_buffer[i].current, file_buffer[i].power);

                    UINT bw;
                    f_write(&fil, buf, strlen(buf), &bw);
                    write_count++;
                }

                buffer_count = 0;
                last_write_tick = osKernelGetTickCount();
                osMutexRelease(sd_mutexHandle);
            }
        }

        osDelay(1);
    }
}
