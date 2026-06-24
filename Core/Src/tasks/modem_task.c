#include "tasks.h"
#include "quectel_drive.h"
#include "console.h"
#include <stdio.h>
#include <string.h>
#include "wdt.h"
#include "ff.h"

void StartModemTask(void *argument) {
    CONS_INFO("[MODEM] Task started (prio=AboveNormal)");

    for (;;) {
        uint32_t flags = osEventFlagsWait(sensor_event_flagsHandle,
                                          EVT_FILE_READY,
                                          osFlagsWaitAny,
                                          osWaitForever);
        CONS_INFO("[MODEM] Woke up from EVT_FILE_READY (flags=0x%08lX)", (unsigned long)flags);

        for (int attempt = 0; attempt < 3; attempt++) {
            char current_file[32] = {0};

            /* Peek at queue head (read without removing — allows retry on failure) */
            int q_ret = upload_queue_peek(current_file, sizeof(current_file));

            if (q_ret != 0 || current_file[0] == '\0') {
                CONS_WARN("[MODEM] Upload queue empty — nothing to upload");
                break;
            }

            CONS_INFO("[MODEM] Uploading '%s' (attempt %d/3)", current_file, attempt + 1);

            upload_busy = 1;
            HAL_StatusTypeDef result = Modem_UploadFile(current_file);
            upload_busy = 0;

            if (result == HAL_OK) {
                CONS_OK("[MODEM] Upload '%s' successful", current_file);
                upload_queue_pop(NULL, 0);  /* Remove from queue */

                /* Create .DONE marker on SD to survive resets */
                osMutexAcquire(sd_mutexHandle, osWaitForever);
                int file_idx = 0;
                if (sscanf(current_file, "TRIG_%d.CSV", &file_idx) == 1) {
                    char done_name[32];
                    snprintf(done_name, sizeof(done_name), "TRIG_%03d.DONE", file_idx);
                    FIL done_f;
                    FRESULT done_fr = f_open(&done_f, done_name, FA_CREATE_NEW | FA_WRITE);
                    if (done_fr == FR_OK) {
                        f_close(&done_f);
                        CONS_DBG("[MODEM] Created marker: %s\r\n", done_name);
                    } else {
                        CONS_WARN("[MODEM] Marker '%s' create failed (FR=%d)\r\n",
                                  done_name, done_fr);
                    }
                }
                osMutexRelease(sd_mutexHandle);

                osEventFlagsSet(sensor_event_flagsHandle, EVT_UPLOAD_DONE);

                /* Power off modem AFTER queue pop + .DONE creation.
                 * El cierre TCP del modem genera pico de corriente en rail 3.3V
                 * que tira NRST bajo. Si esto pasa, el queue ya fue actualizado
                 * y el .DONE ya fue creado — el NRST no pierde estado. */
                Modem_PowerOff();

                char next_file[32] = {0};
                if (upload_queue_peek(next_file, sizeof(next_file)) == 0) {
                    CONS_INFO("[MODEM] More files in queue — continuing");
                    osEventFlagsSet(sensor_event_flagsHandle, EVT_FILE_READY);
                }
                break;

            } else {
                CONS_ERR("[MODEM] Upload '%s' failed (attempt %d/3)", current_file, attempt + 1);
                if (attempt >= 2) {
                    CONS_ERR("[MODEM] All 3 attempts failed for '%s' — skipping", current_file);
                    upload_queue_pop(NULL, 0);  /* Remove failed entry */
                    Modem_PowerOff();  /* Power off after all attempts exhausted */

                    char next_file[32] = {0};
                    if (upload_queue_peek(next_file, sizeof(next_file)) == 0) {
                        CONS_INFO("[MODEM] Next file in queue — continuing");
                        osEventFlagsSet(sensor_event_flagsHandle, EVT_FILE_READY);
                    }
                } else {
                    CONS_INFO("[MODEM] Retrying '%s' in 2s...", current_file);
                    osDelay(2000);
                }
            }
        }

        /* Put modem to sleep if no more files pending */
        char next_check[32] = {0};
        if (upload_queue_peek(next_check, sizeof(next_check)) != 0 || next_check[0] == '\0') {
            CONS_INFO("[MODEM] Upload queue empty, putting modem to sleep\r\n");
            Modem_Sleep();
        }
    }
}
