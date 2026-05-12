#include "tasks.h"
#include "quectel_drive.h"
#include <stdio.h>
#include <string.h>

void StartModemTask(void *argument) {
    printf("[MODEM] Task started (prio=AboveNormal)\r\n");

    for (;;) {
        osEventFlagsWait(sensor_event_flagsHandle,
                         EVT_FILE_READY,
                         osFlagsWaitAny,
                         osWaitForever);

        printf("[MODEM] File ready for upload: %s\r\n", latest_filename);

        HAL_StatusTypeDef result = Modem_UploadFile(latest_filename);

        if (result == HAL_OK) {
            printf("[MODEM] Upload completed successfully\r\n");
            osEventFlagsSet(sensor_event_flagsHandle, EVT_UPLOAD_DONE);
        } else {
            printf("[MODEM] Upload failed (result=%d)\r\n", (int)result);
        }

        osEventFlagsClear(sensor_event_flagsHandle, EVT_FILE_READY);
    }
}
