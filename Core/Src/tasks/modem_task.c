#include "tasks.h"
#include "quectel_drive.h"
#include "console.h"
#include <stdio.h>
#include <string.h>
#include "wdt.h"

void StartModemTask(void *argument) {
    CONS_INFO("[MODEM] Task started (prio=AboveNormal)");

    for (;;) {
        osEventFlagsWait(sensor_event_flagsHandle,
                         EVT_FILE_READY,
                         osFlagsWaitAny,
                         osWaitForever);

        /* Copy filename to local buffer to avoid race with file_task overwriting
         * latest_filename during a concurrent acquisition. */
        char current_file[32];
        strncpy(current_file, latest_filename, sizeof(current_file) - 1);
        current_file[sizeof(current_file) - 1] = '\0';

        CONS_INFO("[MODEM] File ready for upload: %s", current_file);

        /* Set upload_busy BEFORE calling Modem_UploadFile to protect upload_buf
         * from being freed by file_task if a new acquisition starts concurrently.
         * Cleared AFTER Modem_UploadFile returns (success or failure). */
        upload_busy = 1;

        HAL_StatusTypeDef result = Modem_UploadFile(current_file);

        upload_busy = 0;

        if (result == HAL_OK) {
            CONS_OK("[MODEM] Upload completed successfully");
            osEventFlagsSet(sensor_event_flagsHandle, EVT_UPLOAD_DONE);
            /* Clear EVT_FILE_READY to prevent re-uploading the same file.
             * file_task's acqstn_pending mechanism re-sets it if a new
             * acquisition completed during the upload. */
            osEventFlagsClear(sensor_event_flagsHandle, EVT_FILE_READY);
            upload_retry_count = 0;  /* Reset retry count on successful upload */
        } else {
            CONS_ERR("[MODEM] Upload failed (result=%d)", (int)result);
            upload_retry_count++;
            
            if (upload_retry_count <= max_upload_retries) {
                CONS_INFO("[MODEM] Retrying upload (attempt %lu/%lu) from SD...", 
                         (unsigned long)upload_retry_count, (unsigned long)max_upload_retries);
                /* Signal FILE_READY again for retry — data is on SD card */
                osEventFlagsSet(sensor_event_flagsHandle, EVT_FILE_READY);
            } else {
                CONS_ERR("[MODEM] Max retries (%lu) exceeded, upload failed permanently", (unsigned long)max_upload_retries);
            }
        }

    }
}
