#include "tasks.h"
#include "quectel_drive.h"
#include "console.h"
#include <stdio.h>
#include <string.h>
#include "wdt.h"

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
                osEventFlagsSet(sensor_event_flagsHandle, EVT_UPLOAD_DONE);

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
    }
}
