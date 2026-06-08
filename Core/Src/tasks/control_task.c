/*
 * control_task.c
 *
 *  Created on: May 6, 2026
 *      Author: LindUser
 */

#include "tasks.h"
#include "main.h"
#include "usart.h"
#include "adxl355.h"
#include "Sd_spi.h"
#include "ff.h"
#include "quectel_drive.h"
#include "algo/cli_algo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* UART register helper for diagnostic */
static inline uint16_t uart_read_sr(UART_HandleTypeDef *hu) {
    return hu->Instance->SR;
}
static inline uint16_t uart_read_cr1(UART_HandleTypeDef *hu) {
    return hu->Instance->CR1;
}

extern float trigger_g;
extern osMutexId_t uart_mutexHandle;

volatile uint8_t sdbg_abort_acq = 0;

#define UART_CLI_TIMEOUT_MS 100
#define CMD_BUFFER_SIZE 64

static void print_csv_row(const char* r) {
    const char* p = r;
    printf("|");
    for (int col = 0; col < 9; col++) {
        char field[32];
        int fi = 0;
        while (*p && *p != ';' && fi < 31) field[fi++] = *p++;
        field[fi] = '\0';
        if (*p == ';') p++;
        if (col > 0) printf(" %-8s |", field);
        else printf(" %-7s |", field);
    }
    printf("\r\n");
}

static void Show_CSV_Table(const char* filename) {
    FIL tf;
    if (osMutexAcquire(sd_mutexHandle, 5000) != osOK) {
        printf("[FAIL] Could not acquire SD mutex for table display\r\n");
        return;
    }
    if (f_open(&tf, filename, FA_READ) != FR_OK) {
        printf("[FAIL] Cannot open %s for table display\r\n", filename);
        osMutexRelease(sd_mutexHandle);
        return;
    }

    uint32_t fsize = f_size(&tf);
    static char line[160];
    static char first_lines[5][160];
    static char last_lines[5][160];
    uint32_t last_idx = 0;
    uint32_t total = 0;

    memset(first_lines, 0, sizeof(first_lines));
    memset(last_lines, 0, sizeof(last_lines));

    while (f_gets(line, sizeof(line), &tf)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) line[--len] = '\0';
        if (total < 5) strncpy(first_lines[total], line, sizeof(first_lines[0]) - 1);
        strncpy(last_lines[last_idx % 5], line, sizeof(last_lines[0]) - 1);
        last_idx++;
        total++;
    }
    f_close(&tf);

    const char* sep = "+---------+------------+----------+----------+----------+------+------+------+\r\n";

    /* Summary */
    printf("\r\n+============================================+\r\n");
    printf("| SD TEST REPORT                             |\r\n");
    printf("+============================================+\r\n");
    printf("| File: %-37s |\r\n", filename);
    printf("| Size: %-37lu |\r\n", (unsigned long)fsize);
    printf("| Lines: %-36lu |\r\n", (unsigned long)total);
    printf("| Avg bytes/line: %-30.1f |\r\n", total > 0 ? (float)fsize / total : 0.0f);
    printf("+============================================+\r\n\n");

    if (total == 0) { printf("(empty file)\r\n"); osMutexRelease(sd_mutexHandle); return; }

    uint32_t data_total = (total > 0) ? total - 1 : 0;

    /* ASCII table header */
    printf("%s", sep);
    printf("| t_rel(s)| unix_time  | x_g      | y_g      | z_g      | V    | I    | P    |\r\n");
    printf("%s", sep);

    /* First 5 data rows */
    uint32_t max_show = data_total < 10 ? data_total : 5;
    for (uint32_t i = 1; i <= max_show && i < 5; i++) {
        if (strlen(first_lines[i]) > 0) print_csv_row(first_lines[i]);
    }

    if (data_total > 10) {
        printf("|  ...    |  ...       |  ...     |  ...     |  ...     | ...  | ...  | ...  |\r\n");
        uint32_t start = (last_idx >= 5) ? (last_idx % 5) : 0;
        for (uint32_t k = 0; k < 5 && k < data_total; k++) {
            uint32_t idx = (start + k) % 5;
            if (strlen(last_lines[idx]) > 0 && strcmp(last_lines[idx], first_lines[0]) != 0) {
                uint8_t dup = 0;
                for (uint32_t j = 1; j < 5; j++)
                    if (strcmp(last_lines[idx], first_lines[j]) == 0) { dup = 1; break; }
                if (!dup) print_csv_row(last_lines[idx]);
            }
        }
    }

    printf("%s", sep);
    printf("| Samples: %-55lu |\r\n", (unsigned long)data_total);
    printf("%s", sep);
    printf("\n");
    osMutexRelease(sd_mutexHandle);
}

static void Run_SD_Test(void) {
    FIL tf;
    FRESULT res;
    UINT bw;
    const uint32_t adxl_mask = (uint32_t)ADXL_INT1_Pin;

    printf("\r\n=== SD TEST (forced 10s acquisition) ===\r\n");

    /* Mask ADXL_INT1 so sensor task stays idle during the whole test */
    __disable_irq();
    EXTI->IMR &= ~adxl_mask;
    EXTI->PR = adxl_mask;
    __enable_irq();
    osEventFlagsClear(sensor_event_flagsHandle, EVT_MOTION_DETECTED);
    sdbg_abort_acq = 1;

    /* Check if test file already exists */
    res = f_open(&tf, "TEST_DATA.CSV", FA_READ);
    if (res == FR_OK) {
        f_close(&tf);
        printf("[INFO] TEST_DATA.CSV already exists, showing summary.\r\n");
        printf("[INFO] Delete it from SD to run a fresh acquisition.\r\n\n");
        Show_CSV_Table("TEST_DATA.CSV");
        goto cleanup;
    }

    /* Create new test file */
    if (osMutexAcquire(sd_mutexHandle, 5000) != osOK) {
        printf("[FAIL] Could not acquire SD mutex\r\n");
        goto cleanup;
    }

    res = f_open(&tf, "TEST_DATA.CSV", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        printf("[FAIL] f_open(TEST_DATA.CSV) = %d\r\n", res);
        osMutexRelease(sd_mutexHandle);
        goto cleanup;
    }

    char header[] = "timestamp_rel_s;timestamp_abs;unix_time;x_g;y_g;z_g;voltaje;corriente;potencia\r\n";
    f_write(&tf, header, strlen(header), &bw);
    printf("[WRITE] CSV header written (%u bytes)\r\n", bw);

    printf("[ACQ] Acquiring for 10 seconds...\r\n");
    printf("[ACQ] ");

    uint32_t start_tick = osKernelGetTickCount();
    uint32_t sample_count = 0;

    while ((osKernelGetTickCount() - start_tick) < 10000) {
        ADXL355_Data_t data;
        ADXL355_Read_Data(&data);

        uint32_t elapsed = osKernelGetTickCount() - start_tick;
        uint32_t rel_sec = elapsed / 1000;
        uint32_t rel_ms = elapsed % 1000;
        uint32_t abs_sec = 1767817653 + rel_sec;

        char buf[140];
        int n = snprintf(buf, sizeof(buf),
            "%lu.%03lu;%lu.%03lu;%lu.%03lu;%.6f;%.6f;%.6f;0.00;0.00;0.00\r\n",
            rel_sec, rel_ms,
            abs_sec, rel_ms,
            abs_sec, rel_ms,
            data.x_g, data.y_g, data.z_g);
        f_write(&tf, buf, n, &bw);
        sample_count++;

        if (sample_count % 100 == 0) printf(".");
        osDelay(10);
    }
    printf("\r\n");

    uint32_t fsize = f_size(&tf);
    f_close(&tf);
    osMutexRelease(sd_mutexHandle);

    printf("[OK] TEST_DATA.CSV created: %lu samples, %lu bytes (%.1f KB)\r\n",
           (unsigned long)sample_count, (unsigned long)fsize, (float)fsize / 1024.0f);

    Show_CSV_Table("TEST_DATA.CSV");

cleanup:
    /* Clear event flag BEFORE clearing abort flag, so sensor task sees clean state */
    osEventFlagsClear(sensor_event_flagsHandle, EVT_MOTION_DETECTED);
    __disable_irq();
    EXTI->PR = adxl_mask;
    EXTI->IMR |= adxl_mask;
    __enable_irq();
    sdbg_abort_acq = 0;
}

void StartControlTask(void *argument) {
    uint8_t rx_byte;
    char cmd_buffer[CMD_BUFFER_SIZE];
    uint32_t cmd_index = 0;
    const char* prompt = "\r\nHERMES> ";

    printf("[CONTROL] Task started (prio=BelowNormal)\r\n");

    /* UART2 diagnostic at boot */
    uint16_t sr = uart_read_sr(&huart2);
    uint16_t cr1 = uart_read_cr1(&huart2);
    printf("[UART2-DIAG] SR=0x%04X CR1=0x%04X\r\n", (unsigned)sr, (unsigned)cr1);
    printf("[UART2-DIAG] UE=%d RE=%d TE=%d RXNE=%d ORE=%d\r\n",
           (int)((cr1 >> 13) & 1), (int)((cr1 >> 2) & 1), (int)((cr1 >> 3) & 1),
           (int)((sr >> 5) & 1), (int)((sr >> 3) & 1));

    printf("%s", prompt);

    uint32_t poll_loops = 0;
    for (;;) {
        // Drain UART: read all available bytes with zero-wait after first
        uint8_t got_byte = 0;
        do {
            // Clear overrun error before polling
            if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
                __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_ORE);
                printf("[UART2] ORE cleared\r\n");
            }

            uint32_t poll_timeout = got_byte ? 0 : UART_CLI_TIMEOUT_MS;
            HAL_StatusTypeDef uart_ret = HAL_UART_Receive(&huart2, &rx_byte, 1, poll_timeout);
            if (uart_ret != HAL_OK) {
                if (uart_ret == HAL_ERROR && poll_loops % 100 == 0) {
                    printf("[UART2] HAL_ERROR SR=0x%04X\r\n", (unsigned)uart_read_sr(&huart2));
                }
                break;
            }
            got_byte = 1;

            // Echo back (protected by UART mutex to avoid HAL_BUSY)
            if (uart_mutexHandle != NULL) osMutexAcquire(uart_mutexHandle, osWaitForever);
            HAL_UART_Transmit(&huart2, &rx_byte, 1, 10);
            if (uart_mutexHandle != NULL) osMutexRelease(uart_mutexHandle);

            // Process received character
            if (rx_byte == '\r' || rx_byte == '\n') {
                // Null terminate and process command
                cmd_buffer[cmd_index] = '\0';
                if (cmd_index > 0) {
                    CliParseResult_t parsed = cli_parse_cmd(cmd_buffer);
                    switch (parsed.cmd) {
                        case CLI_HELP:
                            printf("\r\nAvailable commands:\r\n");
                            printf("  help    - Show this help\r\n");
                            printf("  status  - Show system status\r\n");
                            printf("  accel   - Read current accelerometer data\r\n");
                            printf("  trigger - Show/set trigger threshold (G)\r\n");
                            printf("  log     - List log files on SD\r\n");
                            printf("  test    - Simulate motion (pipeline test)\r\n");
                            printf("  sdtest  - SD test: 10s forced acquisition + ASCII table\r\n");
                            printf("  modem_on - Power on modem and test AT sync\r\n");
                            break;
                        case CLI_STATUS:
                            printf("\r\nSystem Status:\r\n");
                            printf("  Trigger threshold: %.3f G\r\n", trigger_g);
                            // TODO: Add more status info (task states, SD usage, etc.)
                            break;
                        case CLI_ACCEL: {
                            ADXL355_Data_t data;
                            ADXL355_Read_Data(&data);
                            printf("\r\nAccelerometer:\r\n");
                            printf("  X: %.3f g\r\n", data.x_g);
                            printf("  Y: %.3f g\r\n", data.y_g);
                            printf("  Z: %.3f g\r\n", data.z_g);
                            break;
                        }
                        case CLI_TRIGGER_SET:
                            if (parsed.valid) {
                                trigger_g = parsed.value;
                                printf("\r\nTrigger threshold set to %.3f G\r\n", trigger_g);
                            } else {
                                printf("\r\nInvalid trigger value (must be 0-10 G)\r\n");
                            }
                            break;
                        case CLI_TRIGGER_GET:
                            printf("\r\nCurrent trigger threshold: %.3f G\r\n", trigger_g);
                            break;
                        case CLI_LOG:
                            printf("\r\nLog files: (not implemented)\r\n");
                            // TODO: Implement SD card file listing using sd_mutex
                            break;
                        case CLI_MODEM_ON:
                            printf("\r\n[CMD] Powering on modem (real hardware)...\r\n");
                            {
                                HAL_StatusTypeDef ret = Modem_PowerOn();
                                printf("[CMD] Modem_PowerOn returned: %d\r\n", (int)ret);
                                if (ret == HAL_OK) {
                                    printf("[CMD] Modem ready! Test AT...\r\n");
                                    ret = Modem_SendAT("AT", "OK", 1000);
                                    printf("[CMD] Modem_SendAT(AT) returned: %d\r\n", (int)ret);
                                }
                            }
                            break;
                        case CLI_SDTEST:
                            Run_SD_Test();
                            break;
                        case CLI_TEST:
                            printf("\r\nSimulating motion event...\r\n");
                            printf("  Setting EVT_MOTION_DETECTED -> sensor_task starts\r\n");
                            printf("  -> sensor_task queues data -> file_task writes SD\r\n");
                            printf("  -> modem_task simulates upload (5s)\r\n");
                            osEventFlagsSet(sensor_event_flagsHandle, EVT_MOTION_DETECTED);
                            break;
                        default:
                            printf("\r\nUnknown command: %s\r\n", cmd_buffer);
                            break;
                    }
                    printf("%s", prompt);
                } else {
                    printf("%s", prompt);
                }
                cmd_index = 0;
            } else if (rx_byte >= 32 && rx_byte <= 126 && cmd_index < (CMD_BUFFER_SIZE - 1)) {
                // Printable character
                cmd_buffer[cmd_index++] = rx_byte;
            }
            // Ignore other characters (like delete, etc.) for simplicity
        } while (got_byte && cmd_index < (CMD_BUFFER_SIZE - 1));
        poll_loops++;
        // Small delay to prevent CPU hogging if no data
        osDelay(1);
    }
}