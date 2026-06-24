/*
 * sensor_task.c
 *
 *  Created on: May 6, 2026
 *      Author: LindUser
 *
 *  Writes samples directly to SD during acquisition (no intermediate queue).
 *  After finalization, pushes filename to upload_queue and signals EVT_FILE_READY.
 */
#include "tasks.h"
#include "adxl355.h"
#include "wdt.h"
#include "console.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Globales de sd_spi.c */
extern FATFS fs;
extern FIL fil;

/* Variables globales de main.c */
extern float trigger_g;
extern volatile uint8_t sdbg_abort_acq;

void StartSensorTask(void *argument) {
    ADXL355_Data_t data;
    uint8_t acquiring = 0;
    uint32_t acq_start = 0;
    uint32_t settling_start = 0;
    uint8_t in_settling = 0;
    float prev_mag = -1.0f;
    const float static_delta = 0.005f;
    const uint32_t settling_duration = 3000;
    const uint32_t min_duration = 3000;
    uint32_t read_index = 0;
    uint32_t last_print = 0;
    char filename[32];
    uint32_t write_count = 0;
    static int next_trig_idx = 1;  /* Persistent across acquisitions */
    UINT bw;

    CONS_INFO("[SENSOR] Task started (prio=High)\r\n");

    for (;;) {
        WDT_Refresh();

        /* === Wait for motion trigger: EXTI interrupt + software polling fallback === */
        osEventFlagsClear(sensor_event_flagsHandle, EVT_MOTION_DETECTED);

        uint32_t motion_detected = 0;
        uint32_t evt_flags = 0;

        for (int heartbeat = 50; heartbeat > 0 && !motion_detected; heartbeat--) {
            /* Check hardware interrupt event flag (100ms timeout) */
            evt_flags = osEventFlagsWait(sensor_event_flagsHandle,
                             EVT_MOTION_DETECTED,
                             osFlagsWaitAny,
                             100);
            if (evt_flags & EVT_MOTION_DETECTED) {
                motion_detected = 1;
                break;
            }
            /* Software polling fallback: read sensor, check all 3 axes */
            ADXL355_Data_t poll_data;
            ADXL355_Read_Data(&poll_data);
            float poll_mag = sqrtf(poll_data.x_g * poll_data.x_g +
                                   poll_data.y_g * poll_data.y_g +
                                   poll_data.z_g * poll_data.z_g);
            if (poll_mag > trigger_g * 2.5f) {
                motion_detected = 1;
                CONS_OK("[SENSOR] Motion detected via polling (%.3f G)\r\n",
                       poll_mag);
                break;
            }
            /* Heartbeat cada ~1s (10 iterations) + pet watchdog */
            if (heartbeat % 10 == 0) {
                WDT_Refresh();
                CONS("[SENSOR] Waiting for motion... (%d)\r\n", (heartbeat / 10) - 1);
            }
        }

        /* If sdtest is active, ignore motion events and keep waiting */
        if (sdbg_abort_acq) {
            continue;
        }

        if (!motion_detected) {
            continue;
        }

        /* === Don't start if modem is uploading (SD busy) === */
        if (upload_busy) {
            CONS_WARN("[SENSOR] Upload in progress, deferring acquisition\r\n");
            osDelay(1000);
            continue;
        }

        CONS_OK("[SENSOR] Motion detected, starting acquisition\r\n");
        acquiring = 1;
        acq_start = osKernelGetTickCount();
        settling_start = 0;
        in_settling = 0;
        prev_mag = -1.0f;
        read_index = 0;
        last_print = 0;
        write_count = 0;

        /* === Open CSV file (with SD mutex) === */
        osMutexAcquire(sd_mutexHandle, osWaitForever);

        /* Scan for next available TRIG index from last known position */
        {
            int found_idx = next_trig_idx;
            for (int i = next_trig_idx; i < 999; i++) {
                sprintf(filename, "TRIG_%03d.CSV", i);
                FIL sf;
                FRESULT sfr = f_open(&sf, filename, FA_READ);
                if (sfr != FR_OK) {
                    found_idx = i;
                    break;
                }
                f_close(&sf);
            }
            CONS_INFO("[SENSOR] Next filename: %s\r\n", filename);
            next_trig_idx = found_idx + 1;
        }

        FRESULT fr = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);
        if (fr != FR_OK) {
            CONS_ERR("[SENSOR] Cannot create %s (FR=%d)\r\n", filename, fr);
            osMutexRelease(sd_mutexHandle);
            osDelay(500);
            continue;
        }

        /* Write CSV header */
        char header[] = "timestamp_rel_s;timestamp_abs;unix_time;x_g;y_g;z_g;voltaje;corriente;potencia\r\n";
        f_write(&fil, header, strlen(header), &bw);
        f_sync(&fil);
        osMutexRelease(sd_mutexHandle);

        CONS_OK("[SENSOR] Opened %s for writing\r\n", filename);

        /* === Acquisition loop (direct SD writes, max 15 min) === */
        while (acquiring &&
               (osKernelGetTickCount() - acq_start) < 900000) {

            if (sdbg_abort_acq) {
                CONS_WARN("[SENSOR] Acquisition aborted by sdtest\r\n");
                acquiring = 0;
                break;
            }

            ADXL355_Read_Data_DMA(&data);
            float current_mag = sqrtf(data.x_g * data.x_g +
                                      data.y_g * data.y_g +
                                      data.z_g * data.z_g);

            uint32_t now = osKernelGetTickCount();

            /* Log cada ~125ms */
            if (now - last_print >= 125) {
                CONS("[SENSOR] X:%.3f Y:%.3f Z:%.3f g | T:%.1fs\r\n",
                       data.x_g, data.y_g, data.z_g,
                       (float)(now - acq_start) / 1000.0f);
                last_print = now;
            }

            /* === Write sample directly to SD === */
            osMutexAcquire(sd_mutexHandle, osWaitForever);

            uint32_t rel_ms = now - acq_start;
            uint32_t abs_ms = 1767817653000UL + rel_ms;
            char line[128];
            int line_len = snprintf(line, sizeof(line),
                "%lu.%03lu;%lu.%03lu;%lu.%03lu;%.6f;%.6f;%.6f;%.2f;%.2f;%.2f\r\n",
                (unsigned long)(rel_ms / 1000), (unsigned long)(rel_ms % 1000),
                (unsigned long)(abs_ms / 1000), (unsigned long)(abs_ms % 1000),
                (unsigned long)(abs_ms / 1000), (unsigned long)(abs_ms % 1000),
                data.x_g, data.y_g, data.z_g,
                0.0f, 0.0f, 0.0f);
            FRESULT fr_w = f_write(&fil, line, (UINT)line_len, &bw);
            write_count++;
            if (fr_w != FR_OK) {
                CONS_ERR("[SENSOR] f_write FAILED at sample #%lu (FR=%d) — aborting acquisition\r\n",
                    (unsigned long)write_count, fr_w);
                osMutexRelease(sd_mutexHandle);
                acquiring = 0;
                break;
            }
            osMutexRelease(sd_mutexHandle);

            /* === Settling logic — delta-based === */
            float delta = (prev_mag < 0) ? 0.0f :
                          fabsf(current_mag - prev_mag);
            if (delta < static_delta * 2) {
                if (!in_settling) {
                    in_settling = 1;
                    settling_start = now;
                    CONS_INFO("[SENSOR] Settling started (delta=%.4fG)\r\n", delta);
                }
                if ((now - settling_start) > settling_duration &&
                    (now - acq_start) > min_duration) {
                    CONS_OK("[SENSOR] Event finished (%.1f s, delta=%.4fG)\r\n",
                           (float)(now - acq_start) / 1000.0f, delta);
                    acquiring = 0;
                }
            } else {
                if (in_settling) {
                    CONS_WARN("[SENSOR] Motion resetting settling (delta=%.4fG)\r\n", delta);
                }
                in_settling = 0;
            }
            prev_mag = current_mag;

            /* Earthquake rejection (95% of full scale) */
            float full_scale = ADXL355_Get_Full_Scale();
            float eq_threshold = full_scale * 0.95f;
            if (data.x_g > eq_threshold || data.x_g < -eq_threshold ||
                data.y_g > eq_threshold || data.y_g < -eq_threshold ||
                data.z_g > eq_threshold || data.z_g < -eq_threshold) {
                CONS_ERR("[SENSOR] Earthquake rejected (%.3fG > %.1fG)\r\n",
                       data.x_g > eq_threshold ? data.x_g : -data.x_g, eq_threshold);
                acquiring = 0;
            }
            read_index++;
            osDelay(10);
            WDT_Refresh();
        }

        CONS_INFO("[SENSOR] Acquisition loop exited (acquiring=%d, samples=%lu)\r\n",
            (int)acquiring, (unsigned long)write_count);

        /* === Finalize file === */
        osMutexAcquire(sd_mutexHandle, osWaitForever);

        CONS_INFO("[SENSOR] Finalizing file (%lu bytes, %lu samples)...\r\n",
            (unsigned long)f_size(&fil), (unsigned long)write_count);
        FRESULT fr_sync = f_sync(&fil);
        if (fr_sync != FR_OK) {
            CONS_ERR("[SENSOR] f_sync failed (FR=%d)\r\n", fr_sync);
        }
        FRESULT fr_close = f_close(&fil);
        if (fr_close != FR_OK) {
            CONS_ERR("[SENSOR] f_close failed (FR=%d)\r\n", fr_close);
        }
        osDelay(50);

        /* === VERIFY: re-open for READ to confirm file persists === */
        int verified = 0;
        for (int v_attempt = 0; v_attempt < 3; v_attempt++) {
            FIL vf;
            FRESULT vfr = f_open(&vf, filename, FA_READ);
            if (vfr == FR_OK) {
                CONS_OK("[SENSOR] VERIFY: '%s' reopened OK (%lu bytes)\r\n",
                       filename, (unsigned long)f_size(&vf));
                f_close(&vf);
                verified = 1;
                break;
            }

            if (v_attempt == 0) {
                CONS_ERR("[SENSOR] VERIFY: '%s' READ OPEN FAILED (FR=%d) — will remount and retry\r\n",
                       filename, vfr);

                /* Check f_stat for more info */
                FILINFO sinfo;
                FRESULT sfr = f_stat(filename, &sinfo);
                if (sfr == FR_OK) {
                    CONS_WARN("[SENSOR] f_stat finds '%s' (%lu bytes) but f_open fails! DIRTY CACHE?\r\n",
                           filename, (unsigned long)sinfo.fsize);
                } else {
                    CONS_WARN("[SENSOR] f_stat also fails for '%s' (FR=%d)\r\n", filename, sfr);
                }

                /* Remount to refresh FatFs state */
                CONS_WARN("[SENSOR] Attempting unmount + remount\r\n");
                f_mount(NULL, "0:", 0);
                f_mount(&fs, "0:/", 1);

                /* Directory listing */
                DIR dir;
                FILINFO dno;
                CONS_WARN("[SENSOR] Root directory listing:\r\n");
                if (f_opendir(&dir, "0:/") == FR_OK) {
                    while (f_readdir(&dir, &dno) == FR_OK && dno.fname[0]) {
                        CONS_WARN("  %-20s %10lu bytes\r\n", dno.fname, (unsigned long)dno.fsize);
                    }
                    f_closedir(&dir);
                }
            } else {
                CONS_WARN("[SENSOR] VERIFY retry #%d\r\n", v_attempt + 1);
                osDelay(100);
            }
        }

        if (!verified) {
            CONS_ERR("[SENSOR] VERIFY: '%s' NOT FOUND after 3 attempts — FILE WRITE FAILED!\r\n", filename);
        }

        osMutexRelease(sd_mutexHandle);

        /* === Push to upload queue and signal modem === */
        int q_ok = upload_queue_push(filename);
        if (!q_ok) {
            CONS_ERR("[SENSOR] upload_queue_push('%s') FAILED — queue full!\r\n", filename);
        }

        strncpy(latest_filename, filename, sizeof(latest_filename) - 1);
        latest_filename[sizeof(latest_filename) - 1] = '\0';
        osEventFlagsSet(sensor_event_flagsHandle, EVT_FILE_READY);
        CONS_OK("[SENSOR] EVT_FILE_READY signaled, modem_task will upload '%s'\r\n", latest_filename);
    }
}
