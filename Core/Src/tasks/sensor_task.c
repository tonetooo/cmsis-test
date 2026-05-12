/*
 * sensor_task.c
 *
 *  Created on: May 6, 2026
 *      Author: LindUser
 */
#include "tasks.h"
#include "adxl355.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
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
    printf("[SENSOR] Task started (prio=High)\r\n");
    for (;;) {
        /* Esperar trigger de movimiento con timeout de 5s para heartbeat */
        uint32_t evt_flags = osEventFlagsWait(sensor_event_flagsHandle,
                         EVT_MOTION_DETECTED,
                         osFlagsWaitAny | osFlagsNoClear,
                         5000);
        /* If sdtest is active, ignore motion events and keep waiting */
        if (sdbg_abort_acq) {
            osEventFlagsClear(sensor_event_flagsHandle, EVT_MOTION_DETECTED);
            printf("[SENSOR] Waiting for motion...\r\n");
            continue;
        }

        if (!(evt_flags & EVT_MOTION_DETECTED)) {
            printf("[SENSOR] Waiting for motion...\r\n");
            continue;
        }
        /* Now clear the flag */
        osEventFlagsClear(sensor_event_flagsHandle, EVT_MOTION_DETECTED);
        printf("[SENSOR] Motion detected, starting acquisition\r\n");
        acquiring = 1;
        acq_start = osKernelGetTickCount();
        settling_start = 0;
        in_settling = 0;
        prev_mag = -1.0f;
        read_index = 0;
        last_print = 0;
        /* Loop de adquisicion (max 15 minutos = 900000 ms) */
        while (acquiring &&
               (osKernelGetTickCount() - acq_start) < 900000) {
            if (sdbg_abort_acq) {
                printf("[SENSOR] Acquisition aborted by sdtest\r\n");
                acquiring = 0;
                break;
            }
            ADXL355_Read_Data(&data);
            float current_mag = sqrtf(data.x_g * data.x_g +
                                      data.y_g * data.y_g);
            /* Log cada ~125ms */
            uint32_t now = osKernelGetTickCount();
            if (now - last_print >= 125) {
                printf("[SENSOR] X:%.3f Y:%.3f Z:%.3f g | T:%.1fs\r\n",
                       data.x_g, data.y_g, data.z_g,
                       (float)(now - acq_start) / 1000.0f);
                last_print = now;
            }
            /* Encolar dato para file_task */
            SensorReading_t reading;
            reading.timestamp_ms = now;
            reading.x_g = data.x_g;
            reading.y_g = data.y_g;
            reading.z_g = data.z_g;
            reading.voltage = 0.0f;
            reading.current = 0.0f;
            reading.power = 0.0f;
            osMessageQueuePut(sensor_queueHandle, &reading, 0, 0);
            /* Settling logic */
            if (current_mag < trigger_g) {
                if (!in_settling) {
                    in_settling = 1;
                    settling_start = now;
                    printf("[SENSOR] Settling started (<%.2f G)\r\n",
                           trigger_g);
                }
                float delta = (prev_mag < 0) ? 0.0f :
                              fabsf(current_mag - prev_mag);
                if (delta > static_delta) {
                    settling_start = now;
                }
                if ((now - settling_start) > settling_duration &&
                    (now - acq_start) > min_duration) {
                    printf("[SENSOR] Event finished (%.1f s)\r\n",
                           (float)(now - acq_start) / 1000.0f);
                    acquiring = 0;
                }
            } else {
                if (in_settling) {
                    printf("[SENSOR] New motion, resetting settling\r\n");
                }
                in_settling = 0;
            }
            prev_mag = current_mag;
            /* Earthquake rejection */
            if (data.x_g > 2.0f || data.x_g < -2.0f ||
                data.y_g > 2.0f || data.y_g < -2.0f ||
                data.z_g > 2.0f || data.z_g < -2.0f) {
                printf("[SENSOR] Earthquake rejected (>2G)\r\n");
                acquiring = 0;
            }
            read_index++;
            osDelay(10); /* Precisos 10ms */
        }
        /* Avisar a modem_task que hay datos disponibles */
        if (!acquiring) {
            osEventFlagsSet(sensor_event_flagsHandle, EVT_ACQSTN_DONE);
        }
    }
}


