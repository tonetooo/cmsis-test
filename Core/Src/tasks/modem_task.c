#include "tasks.h"
#include "quectel_drive.h"
#include <stdio.h>
#include <string.h>

void StartModemTask(void *argument) {
    printf("[MDM] START prio=ABOVE-NORMAL\r\n");

    const uint8_t MAX_RETRIES = 3;
    const uint32_t BASE_DELAY_MS = 1000;
    const int8_t MIN_RSSI = 10;

    for (;;) {
        osEventFlagsWait(sensor_event_flagsHandle,
                         EVT_FILE_READY,
                         osFlagsWaitAny,
                         osWaitForever);

        printf("[MDM] UPLOAD %s\r\n", latest_filename);

        int8_t rssi = Modem_GetSignalQuality();
        if (rssi < MIN_RSSI) {
            printf("[MDM] LOW-RSSI rssi=%d min=%d\r\n",
                   rssi, MIN_RSSI);
        } else {
            printf("[MDM] RSSI=%d\r\n", rssi);
        }

        HAL_StatusTypeDef result = HAL_ERROR;
        uint8_t attempt = 0;

        while (attempt < MAX_RETRIES && result != HAL_OK) {
            if (attempt > 0) {
                uint32_t delay_ms = BASE_DELAY_MS * (1 << (attempt - 1));
                printf("[MDM] RETRY %d/%d wait=%lums\r\n", attempt, MAX_RETRIES, delay_ms);
                osDelay(delay_ms);
            }

            result = Modem_UploadFile(latest_filename);
            attempt++;
        }

        if (result == HAL_OK) {
            printf("[MDM] UPLOAD-OK\r\n");
            osEventFlagsSet(sensor_event_flagsHandle, EVT_UPLOAD_DONE);
        } else {
            printf("[MODEM] Upload failed after %d attempts (result=%d)\r\n",
                   MAX_RETRIES, (int)result);
        }

        osEventFlagsClear(sensor_event_flagsHandle, EVT_FILE_READY);
    }
}
