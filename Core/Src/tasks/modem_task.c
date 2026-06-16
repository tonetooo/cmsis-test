#include "tasks.h"
#include "quectel_drive.h"
#include "console.h"
#include <stdio.h>
#include <string.h>

void StartModemTask(void *argument) {
    CONS_INFO("[MODEM] Task started (prio=AboveNormal)");

    for (;;) {
        osEventFlagsWait(sensor_event_flagsHandle,
                         EVT_FILE_READY,
                         osFlagsWaitAny,
                         osWaitForever);

        CONS_INFO("[MODEM] File ready for upload: %s", latest_filename);

        HAL_StatusTypeDef result = Modem_UploadFile(latest_filename);

        if (result == HAL_OK) {
            CONS_OK("[MODEM] Upload completed successfully");
            osEventFlagsSet(sensor_event_flagsHandle, EVT_UPLOAD_DONE);
        } else {
            CONS_ERR("[MODEM] Upload failed (result=%d)", (int)result);
        }

        osEventFlagsClear(sensor_event_flagsHandle, EVT_FILE_READY);
    }
}
