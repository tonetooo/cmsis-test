#include "tasks.h"
#include "wdt.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include "console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

extern FATFS fs;
extern FIL fil;
extern uint8_t sd_reinit(void);  // Full SD hardware re-init

    char latest_filename[32] = {0};
    uint8_t *upload_buf = NULL;
    uint32_t upload_buf_size = 0;

    /* Local buffer for failed uploads */
    #define LOCAL_BUF_CAPACITY (32 * 1024)  /* 32KB local buffer (fits in heap) */
    uint8_t *local_buf = NULL;
    uint32_t local_buf_size = 0;
    uint32_t local_buf_max_size = LOCAL_BUF_CAPACITY;
    uint32_t upload_retry_count = 0;
    uint32_t max_upload_retries = 3;
    volatile uint8_t upload_busy = 0;

    /* FR_DENIED/LOCKED tracking — prevents infinite re-init loop on corrupted entries */
    #define MAX_DENIED_RETRIES 3
    uint8_t denied_retry_count = 0;
    int last_denied_index = -1;

void StartFileTask(void *argument) {
    CONS_INFO("[FILE] Task started (prio=Normal)");
    CONS_INFO("[FILE] Waiting for sensor data on queue...");

    char filename[32];
    uint8_t file_open = 0;
    uint32_t last_write_tick = 0;
    uint32_t write_count = 0;
    uint32_t mutex_acquire_count = 0;
    uint32_t mutex_contention_count = 0;
    uint8_t create_fail_count = 0;
    uint8_t acqstn_pending = 0;  /* EVT_ACQSTN_DONE arrived while file was closed (e.g., during upload) */
    int scan_start = 1;

    /* Pre-allocate binary upload buffer (28 bytes/sample, 32KB = 1170 samples).
     * Each sample is accumulated DURING writing to SD — no f_lseek needed.
     * Backend receives binary and converts to CSV. */
    uint32_t bin_offset = 0;  /* Current write position in binary buffer */
    uint32_t bin_capacity = UPLOAD_BUF_CAPACITY;

    /* Local buffer allocated on-demand after acquisition (to avoid starving upload_buf) */

    for (;;) {
        WDT_Refresh();
        SensorReading_t reading;
        osStatus_t status;

        /* During upload with no active file, skip queue reads to let data
         * accumulate. When upload completes, the backlog will be drained
         * and written to the new acquisition file. */
        if (!file_open && upload_busy) {
            status = osErrorResource;
            osDelay(5);
        } else {
            status = osMessageQueueGet(sensor_queueHandle, &reading, NULL, 200);
        }

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
                /* Free previous upload buffer if any — but only if modem_task
                 * is not currently using it (upload_busy protection against
                 * use-after-free when new acquisition starts during upload). */
                if (upload_buf != NULL) {
                    if (upload_busy) {
                        CONS_DBG("[FILE] Upload in progress, preserving previous upload_buf");
                    } else {
                        vPortFree(upload_buf);
                        upload_buf = NULL;
                        upload_buf_size = 0;
                    }
                }
                bin_offset = 0;

                /* Pre-allocate binary buffer for this acquisition */
                if (upload_buf == NULL && !upload_busy) {
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

                /* Find next available filename — skip indices that previously failed creation.
                 * Use local FIL to avoid cross-contamination with global 'fil' handle. */
                CONS_INFO("[FILE] Scanning for next available TRIG_XXX.CSV (start=%d)...", scan_start);
                for (int i = scan_start; i < 999; i++) {
                    sprintf(filename, "TRIG_%03d.CSV", i);
                    FIL scan_fil;
                    FRESULT scan_fr = f_open(&scan_fil, filename, FA_READ);
                    if (scan_fr != FR_OK) break;
                    FRESULT close_fr = f_close(&scan_fil);
                    if (close_fr != FR_OK) {
                        CONS_WARN("[FILE] f_close on %s returned FR=%d during scan", filename, close_fr);
                    }
                }
                CONS_DBG("[FILE] Next filename: %s", filename);

                WDT_Refresh();

                FRESULT fr = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);
                if (fr != FR_OK) {
                    CONS_WARN("[FILE] CREATE_ALWAYS failed on %s (FR=%d) — attempting f_unlink", filename, fr);
                    FRESULT ur = f_unlink(filename);
                    CONS_WARN("[FILE] f_unlink(%s) = %d", filename, ur);
                    if (ur != FR_OK) {
                        int idx = 0;
                        if (sscanf(filename, "TRIG_%d.CSV", &idx) == 1) {
                            scan_start = idx + 1;
                            CONS_WARN("[FILE] Skipping index %d, next scan from %d", idx, scan_start);
                        }
                    } else {
                        CONS_OK("[FILE] f_unlink succeeded, will retry %s", filename);
                    }
                    osDelay(50);
                    WDT_Refresh();
                }

                fr = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);
                if (fr == FR_OK) {
                    /* Write CSV header */
                    char header[] = "timestamp_rel_s;timestamp_abs;unix_time;x_g;y_g;z_g;voltaje;corriente;potencia\r\n";
                    UINT bw;
                    f_write(&fil, header, strlen(header), &bw);
                    CONS_DBG("[FILE][CSV] Header written (%u bytes)", bw);
                    f_sync(&fil);
                    file_open = 1;
                    write_count = 0;
                    create_fail_count = 0;  /* Reset on success */
                    acqstn_pending = 0;    /* Pending acquisition is now being handled */
                    CONS_OK("[FILE] Opened %s for writing", filename);
                } else {
                    create_fail_count++;
                    CONS_ERR("[FILE] Could not create %s (fail #%lu, FR=%d)",
                             filename, (unsigned long)create_fail_count, fr);

                    /* FR_DENIED (7) / FR_LOCKED (16) = SD write protection, lock, or hardware issue.
                     * Tracks retries per-index to detect ghost/corrupted directory entries. */
                    if (fr == FR_DENIED || fr == FR_LOCKED) {
                        int fail_idx = 0;
                        sscanf(filename, "TRIG_%d.CSV", &fail_idx);

                        if (fail_idx == last_denied_index && fail_idx > 0) {
                            if (denied_retry_count < MAX_DENIED_RETRIES)
                                denied_retry_count++;
                        } else {
                            denied_retry_count = 0;
                            last_denied_index = fail_idx;
                        }

                        CONS_WARN("[FILE] FR=%d on %s (denied_retry=%d/%d)",
                                  fr, filename, denied_retry_count, MAX_DENIED_RETRIES);

                        if (denied_retry_count >= MAX_DENIED_RETRIES) {
                            /* Same index persistently denied — ghost/corrupted entry.
                             * Skip it permanently and refresh FatFs directly (no HW re-init). */
                            CONS_ERR("[FILE] %s persistently DENIED — ghost entry, skipping permanently", filename);
                            scan_start = (fail_idx > 0) ? (fail_idx + 1) : (scan_start + 1);
                            denied_retry_count = 0;
                            last_denied_index = -1;
                            /* Fresh FatFs remount without HW re-init */
                            f_mount(NULL, "0:/", 0);
                            osDelay(50);
                            f_mount(&fs, "0:/", 1);
                            CONS_WARN("[FILE] FatFs fresh remount, next scan from %d", scan_start);
                        } else {
                            /* Full SD hardware re-init (first N-1 attempts) */
                            CONS_WARN("[FILE] FR=%d — triggering SD hardware re-init...", fr);
                            osMutexRelease(sd_mutexHandle);
                            uint8_t reinit_result = sd_reinit();
                            osDelay(200);
                            if (reinit_result == 0) {
                                f_mount(NULL, "0:/", 0);
                                osDelay(50);
                                f_mount(&fs, "0:/", 1);
                                CONS_OK("[FILE] SD re-init + FatFs fresh remount successful");
                            } else {
                                CONS_ERR("[FILE] SD re-init FAILED after max attempts");
                                CONS_ERR("[FILE] SD card may be write-protected or damaged");
                                f_mount(&fs, "0:/", 1);
                                osMutexAcquire(sd_mutexHandle, osWaitForever);
                                while (1) {
                                    CONS_ERR("[FILE] FATAL: Cannot write to SD — system halted");
                                    osDelay(5000);
                                    WDT_Refresh();
                                }
                            }
                            osMutexAcquire(sd_mutexHandle, osWaitForever);
                            create_fail_count = 0;
                            scan_start = 1;
                            CONS_OK("[FILE] SD online, will retry from TRIG_001");
                        }
                    }
                    /* After 3 consecutive failures (non-DENIED), force FatFs remount */
                    else if (create_fail_count >= 3) {
                        CONS_WARN("[FILE] %lu consecutive create failures — remounting FatFs...",
                                  (unsigned long)create_fail_count);
                        osMutexRelease(sd_mutexHandle);
                        f_mount(NULL, "0:/", 0);
                        osDelay(100);
                        f_mount(&fs, "0:/", 1);
                        osMutexAcquire(sd_mutexHandle, osWaitForever);
                        create_fail_count = 0;
                        CONS_OK("[FILE] FatFs remounted, will retry next iteration");
                    }

                    /* Rate-limit: wait 500ms before retrying */
                    osDelay(500);
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

        /* Acquisition completed while file was closed (e.g., during modem upload).
         * Don't clear EVT_ACQSTN_DONE — it persists until file_task opens the new
         * acquisition's file and finalizes normally. */
        if (!file_open && (flags & EVT_ACQSTN_DONE)) {
            if (!acqstn_pending) {
                acqstn_pending = 1;
                CONS_INFO("[FILE] ACQSTN_DONE during upload — pending file open (will process when upload completes)");
            }
        }

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

                    /* Force fresh FatFs directory read so this file is visible
                     * to the next scan loop. Otherwise FatFs cache may hide the
                     * just-closed file, and the scan will reuse the same index. */
                    osDelay(50);
                    f_mount(NULL, "0:/", 0);
                    osDelay(20);
                    f_mount(&fs, "0:/", 1);

                    /* Verify the closed file is visible on SD */
                    FIL vf;
                    int verify_ok = 0;
                    for (int tries = 0; tries < 3; tries++) {
                        if (f_open(&vf, filename, FA_READ) == FR_OK) {
                            f_close(&vf);
                            verify_ok = 1;
                            break;
                        }
                        osDelay(50);
                    }
                    if (!verify_ok) {
                        int idx = 0;
                        sscanf(filename, "TRIG_%d.CSV", &idx);
                        CONS_ERR("[FILE][TRIG] f_open(FA_READ) on freshly closed %s FAILED "
                                 "after remount+retry — file not persistent! "
                                 "Skipping index %d in next scan.", filename, idx);
                        if (idx > 0) scan_start = idx + 1;
                    }

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

                    /* Keep original .CSV filename for both RAM and SD uploads */
                    strncpy(latest_filename, filename, sizeof(latest_filename) - 1);
                    latest_filename[sizeof(latest_filename) - 1] = '\0';

                    osEventFlagsSet(sensor_event_flagsHandle, EVT_FILE_READY);
                    CONS_OK("[FILE] EVT_FILE_READY signaled, modem_task will upload '%s'", latest_filename);
                    upload_retry_count = 0;

                    /* Clear ACQSTN_DONE so sensor_task knows file_task has finished */
                    osEventFlagsClear(sensor_event_flagsHandle, EVT_ACQSTN_DONE);

                    file_open = 0;
                }
            }
        }

        osDelay(1);
    }
}
