/*
 * sensor_task.c
 *
 *  Created on: May 6, 2026
 *      Author: LindUser
 */
#include "tasks.h"
#include "adxl355.h"
#include "wdt.h"
#include "console.h"
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
    CONS_INFO("[SENSOR] Task started (prio=High)\r\n");
    for (;;) {
        WDT_Refresh();
        /* Wait for motion trigger: EXTI interrupt + software polling fallback */
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
             /* Heartbeat each ~1s (10 iterations) + pet watchdog */
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
        /* Flag was auto-cleared by osEventFlagsWait (no osFlagsNoClear) */
        CONS_OK("[SENSOR] Motion detected, starting acquisition\r\n");
        acquiring = 1;
        acq_start = osKernelGetTickCount();
        settling_start = 0;
        in_settling = 0;
        prev_mag = -1.0f;
        read_index = 0;
        last_print = 0;
        CONS_INFO("[SENSOR] Entering acquisition loop\r\n");
        /* Loop de adquisicion (max 15 minutos = 900000 ms) */
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
            /* Log cada ~125ms */
            uint32_t now = osKernelGetTickCount();
            if (now - last_print >= 125) {
                CONS("[SENSOR] X:%.3f Y:%.3f Z:%.3f g | T:%.1fs\r\n",
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
            osStatus_t q_ret = osMessageQueuePut(sensor_queueHandle, &reading, 0, 0);
            if (q_ret != osOK) {
                CONS_ERR("[SENSOR] Queue FULL — sample dropped (ret=%d, queued=%lu)", q_ret, osMessageQueueGetCount(sensor_queueHandle));
            }
            /* Settling logic - delta-based, independiente del offset absoluto */
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
            /* Earthquake rejection (95% of full scale to avoid noise false-triggers) */
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
            osDelay(10); /* Precisos 10ms */
            WDT_Refresh();
        }
        CONS_INFO("[SENSOR] Acquisition loop exited (acquiring=%d)\r\n", (int)acquiring);
        /* Avisar a modem_task que hay datos disponibles */
        if (!acquiring) {
            osEventFlagsSet(sensor_event_flagsHandle, EVT_ACQSTN_DONE);
        }
    }
}


