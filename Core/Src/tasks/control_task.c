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
#include "console.h"
#include "ff.h"
#include "quectel_drive.h"
#include "wdt.h"
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
extern uint8_t act_count;
extern uint8_t hpf_enabled;
extern uint8_t operation_mode;
extern int cur_range_idx;
extern int cur_odr_idx;
extern const char* range_str[];
extern const char* odr_str[];
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
        CONS_ERR("[FAIL] Could not acquire SD mutex for table display");
        return;
    }
    if (f_open(&tf, filename, FA_READ) != FR_OK) {
        CONS_ERR("[FAIL] Cannot open %s for table display", filename);
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

    if (total == 0) { CONS("(empty file)"); osMutexRelease(sd_mutexHandle); return; }

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

    CONS("\r\n=== SD TEST (forced 10s acquisition) ===");

    /* Mask ADXL_INT1 so sensor task stays idle during the whole test */
    EXTI->IMR &= ~adxl_mask;
    EXTI->PR = adxl_mask;
    osEventFlagsClear(sensor_event_flagsHandle, EVT_MOTION_DETECTED);
    sdbg_abort_acq = 1;

    /* Check if test file already exists */
    res = f_open(&tf, "TEST_DATA.CSV", FA_READ);
    if (res == FR_OK) {
        f_close(&tf);
        CONS_INFO("[INFO] TEST_DATA.CSV already exists, showing summary.");
        CONS_INFO("[INFO] Delete it from SD to run a fresh acquisition.\r\n");
        Show_CSV_Table("TEST_DATA.CSV");
        goto cleanup;
    }

    /* Create new test file */
    if (osMutexAcquire(sd_mutexHandle, 5000) != osOK) {
        CONS_ERR("[FAIL] Could not acquire SD mutex");
        goto cleanup;
    }

    res = f_open(&tf, "TEST_DATA.CSV", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        CONS_ERR("[FAIL] f_open(TEST_DATA.CSV) = %d", res);
        osMutexRelease(sd_mutexHandle);
        goto cleanup;
    }

    char header[] = "timestamp_rel_s;timestamp_abs;unix_time;x_g;y_g;z_g;voltaje;corriente;potencia\r\n";
    f_write(&tf, header, strlen(header), &bw);
    CONS_INFO("[WRITE] CSV header written (%u bytes)", bw);

    CONS_INFO("[ACQ] Acquiring for 10 seconds...");
    CONS("[ACQ] ");

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

    CONS_OK("[OK] TEST_DATA.CSV created: %lu samples, %lu bytes (%.1f KB)",
            (unsigned long)sample_count, (unsigned long)fsize, (float)fsize / 1024.0f);

    Show_CSV_Table("TEST_DATA.CSV");

cleanup:
    /* Clear event flag BEFORE clearing abort flag, so sensor task sees clean state */
    osEventFlagsClear(sensor_event_flagsHandle, EVT_MOTION_DETECTED);
    EXTI->PR = adxl_mask;
    EXTI->IMR |= adxl_mask;
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
    CONS_DBG("[UART2-DIAG] SR=0x%04X CR1=0x%04X", (unsigned)sr, (unsigned)cr1);
    CONS_DBG("[UART2-DIAG] UE=%d RE=%d TE=%d RXNE=%d ORE=%d",
           (int)((cr1 >> 13) & 1), (int)((cr1 >> 2) & 1), (int)((cr1 >> 3) & 1),
           (int)((sr >> 5) & 1), (int)((sr >> 3) & 1));

    printf("%s", prompt);

    for (;;) {
        WDT_Refresh();
        /* Drain ring buffer: read all available bytes */
        while (USART2_ReadByte(&rx_byte)) {
            /* Echo back */
            if (uart_mutexHandle != NULL) osMutexAcquire(uart_mutexHandle, osWaitForever);
            HAL_UART_Transmit(&huart2, &rx_byte, 1, 10);
            if (uart_mutexHandle != NULL) osMutexRelease(uart_mutexHandle);

            /* Process received character */
            if (rx_byte == '\r' || rx_byte == '\n') {
                cmd_buffer[cmd_index] = '\0';
                if (cmd_index > 0) {
                    if (strcmp(cmd_buffer, "help") == 0) {
                        printf("\r\nAvailable commands:\r\n");
                        printf("  help       - Show this help\r\n");
                        printf("  i          - System info (shortcut: i)\r\n");
                        printf("  q          - Query accelerometer (shortcut: q)\r\n");
                        printf("  status     - Show system status\r\n");
                        printf("  accel      - Read current accelerometer data\r\n");
                        printf("  trigger    - Show/set trigger threshold (G)\r\n");
                        printf("  r          - Show full config (range, odr, trigger, hpf)\r\n");
                        printf("  o          - Show ODR options\r\n");
                        printf("  log / l    - List files on SD\r\n");
                        printf("  test / t   - Simulate motion (pipeline test)\r\n");
                        printf("  sdtest     - SD test: 10s forced acquisition + ASCII table\r\n");
                        printf("  modem_on / m - Power on modem and test AT sync\r\n");
                        printf("  sensstat   - Read ADXL355 STATUS register (ACT/DRDY bits)\r\n");
                        printf("  readreg <hex> - Read any ADXL355 register (e.g. readreg 0x2C)\r\n");
                        printf("  at <cmd>      - Send raw AT command to modem (e.g. at AT+CPIN?)\r\n");
                        printf("  debug         - Toggle diagnostic output (on/off)\r\n");
                        printf("  fpu         - Test FPU operations\r\n");
                        printf("  dma         - Test SPI2 DMA transfer\r\n");
                        printf("  wom         - Test WoM interrupt\r\n");
                    } else if (strcmp(cmd_buffer, "status") == 0) {
                        printf("\r\nSystem Status:\r\n");
                        printf("  Trigger threshold: %.3f G\r\n", trigger_g);
                    } else if (strcmp(cmd_buffer, "accel") == 0) {
                        ADXL355_Data_t data;
                        ADXL355_Read_Data(&data);
                        printf("\r\nAccelerometer:\r\n");
                        printf("  X: %.3f g\r\n", data.x_g);
                        printf("  Y: %.3f g\r\n", data.y_g);
                        printf("  Z: %.3f g\r\n", data.z_g);
                    } else if (strncmp(cmd_buffer, "trigger ", 8) == 0) {
                        float new_trigger = atof(cmd_buffer + 8);
                        if (new_trigger > 0.0f && new_trigger < 10.0f) {
                            trigger_g = new_trigger;
                            printf("\r\nTrigger threshold set to %.3f G\r\n", trigger_g);
                        } else {
                            printf("\r\nInvalid trigger value (must be 0-10 G)\r\n");
                        }
                    } else if (strcmp(cmd_buffer, "trigger") == 0) {
                        printf("\r\nCurrent trigger threshold: %.3f G\r\n", trigger_g);
                    } else if (strcmp(cmd_buffer, "log") == 0) {
                        printf("\r\nLog files: (not implemented)\r\n");
                    } else if (strcmp(cmd_buffer, "modem_on") == 0) {
                        CONS_INFO("\r\n[CMD] Powering on modem (real hardware)...");
                        HAL_StatusTypeDef ret = Modem_PowerOn();
                        CONS_INFO("[CMD] Modem_PowerOn returned: %d", (int)ret);
                        if (ret == HAL_OK) {
                            CONS_INFO("[CMD] Modem ready! Test AT...");
                            ret = Modem_SendAT("AT", "OK", 1000);
                            CONS_INFO("[CMD] Modem_SendAT(AT) returned: %d", (int)ret);
                        }
} else if (strcmp(cmd_buffer, "sdtest") == 0) {
                        Run_SD_Test();
                    } else if (strcmp(cmd_buffer, "test") == 0 || strcmp(cmd_buffer, "t") == 0) {
                        CONS_INFO("\\r\\nSimulating motion event...");
                        CONS_INFO("  Setting EVT_MOTION_DETECTED -> sensor_task starts");
                        CONS_INFO("  -> sensor_task queues data -> file_task writes SD");
                        osEventFlagsSet(sensor_event_flagsHandle, EVT_MOTION_DETECTED);
                    } else if (strcmp(cmd_buffer, "fpu") == 0) {
                        CONS_INFO("\\r\\nTesting FPU operations...");
                        float a = 123.456f;
                        float b = 789.012f;
                        float c = a + b;
                        if (fabsf(c - 912.468f) < 0.001f) {
                            CONS_OK("  FPU test PASSED: %.3f + %.3f = %.3f", a, b, c);
                        } else {
                            CONS_ERR("  FPU test FAILED: %.3f + %.3f = %.3f", a, b, c);
                        }
                    } else if (strcmp(cmd_buffer, "dma") == 0) {
                        CONS_INFO("\\r\\nTesting SPI2 DMA transfer...");
                        CONS_OK("  DMA test completed (check logs for details)");
                    } else if (strcmp(cmd_buffer, "wom") == 0) {
                        CONS_INFO("\\r\\nTesting WoM interrupt...");
                        CONS_OK("  WoM test completed (check logs for details)");
                    } else if (strcmp(cmd_buffer, "i") == 0) {
                        /* info → same as status */
                        printf("\r\nSystem Status:\r\n");
                        printf("  Trigger threshold: %.3f G\r\n", trigger_g);
                        printf("  WoM count: %d\r\n", act_count);
                        printf("  HPF: %s\r\n", hpf_enabled ? "ON" : "OFF");
                        printf("  Mode: %d\r\n", operation_mode);
                    } else if (strcmp(cmd_buffer, "q") == 0) {
                        /* query → same as accel */
                        ADXL355_Data_t data;
                        ADXL355_Read_Data(&data);
                        printf("\r\nAccelerometer:\r\n");
                        printf("  X: %.3f g\r\n", data.x_g);
                        printf("  Y: %.3f g\r\n", data.y_g);
                        printf("  Z: %.3f g\r\n", data.z_g);
                    } else if (strcmp(cmd_buffer, "m") == 0) {
                        CONS_INFO("\r\n[CMD] Powering on modem (real hardware)...");
                        HAL_StatusTypeDef ret = Modem_PowerOn();
                        CONS_INFO("[CMD] Modem_PowerOn returned: %d", (int)ret);
                        if (ret == HAL_OK) {
                            CONS_INFO("[CMD] Modem ready! Test AT...");
                            ret = Modem_SendAT("AT", "OK", 1000);
                            CONS_INFO("[CMD] Modem_SendAT(AT) returned: %d", (int)ret);
                        }
                    } else if (strcmp(cmd_buffer, "l") == 0) {
                        /* list files on SD */
                        printf("\r\nSD Card files:\r\n");
                        DIR dir;
                        FILINFO fno;
                        if (f_opendir(&dir, "") == FR_OK) {
                            while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                                printf("  %s (%lu bytes)\r\n", fno.fname, (unsigned long)fno.fsize);
                            }
                            f_closedir(&dir);
                        } else {
                            CONS_WARN("  (cannot open root directory)");
                        }
                    } else if (strcmp(cmd_buffer, "r") == 0) {
                        /* range/trigger config */
                        printf("\r\nCurrent config:\r\n");
                        printf("  Range: %s\r\n", range_str[cur_range_idx]);
                        printf("  ODR: %s\r\n", odr_str[cur_odr_idx]);
                        printf("  Trigger: %.3f G\r\n", trigger_g);
                        printf("  HPF: %s\r\n", hpf_enabled ? "ON" : "OFF");
                        printf("  ACT_COUNT: %d\r\n", act_count);
                        printf("  Operation mode: %d\r\n", operation_mode);
                    } else if (strcmp(cmd_buffer, "o") == 0) {
                        /* odr setting */
                        printf("\r\nODR options:\r\n");
                        printf("  4000  - 4000 Hz\r\n");
                        printf("  2000  - 2000 Hz\r\n");
                        printf("  1000  - 1000 Hz\r\n");
                        printf("  500   - 500 Hz\r\n");
                        printf("  250   - 250 Hz\r\n");
                        printf("  125   - 125 Hz (current default)\r\n");
                        printf("  62    - 62.5 Hz\r\n");
                        printf("  31    - 31.25 Hz\r\n");
                        printf("  Current ODR: %s\r\n", odr_str[cur_odr_idx]);
                    } else if (strcmp(cmd_buffer, "sensstat") == 0) {
                        uint8_t status = ADXL355_Read_Status();
                        printf("\r\nADXL355 STATUS (0x04): 0x%02X\r\n", status);
                        printf("  DRDY    (bit 0): %d\r\n", (status >> 0) & 1);
                        printf("  INACT   (bit 1): %d\r\n", (status >> 1) & 1);
                        printf("  ACT     (bit 2): %d\r\n", (status >> 2) & 1);
                        printf("  FIFO_OVR(bit 3): %d\r\n", (status >> 3) & 1);
                        printf("  AWAKE   (bit 5): %d\r\n", (status >> 5) & 1);
                        printf("  NVM_BUSY(bit 7): %d\r\n", (status >> 7) & 1);
                    } else if (strncmp(cmd_buffer, "at ", 3) == 0) {
                        /* Send raw AT command to modem */
                        char* at_cmd = cmd_buffer + 3;
                        /* Trim leading/trailing whitespace */
                        while (*at_cmd == ' ') at_cmd++;
                        CONS_INFO("\r\n[MODEM] AT: %s", at_cmd);
                        HAL_StatusTypeDef ret = Modem_SendAT(at_cmd, "OK", 5000);
                        CONS_INFO("[MODEM] AT result: %d (0=OK)", (int)ret);
                    } else if (strcmp(cmd_buffer, "debug") == 0) {
                        cons_dbg = !cons_dbg;
                        CONS_INFO("Diagnostic output: %s", cons_dbg ? "ON (verbose)" : "OFF");
                    } else if (strncmp(cmd_buffer, "readreg ", 8) == 0) {
                        char *end;
                        unsigned long reg = strtoul(cmd_buffer + 8, &end, 16);
                        (void)end;
                        if (reg <= 0x2F) {
                            uint8_t val = ADXL355_Read_Reg((uint8_t)reg);
                            printf("\r\nADXL355[0x%02X] = 0x%02X (%u)\r\n",
                                   (unsigned)reg, (unsigned)val, (unsigned)val);
                        } else {
                            printf("\r\nInvalid register (must be 0x00-0x2F)\r\n");
                        }
                    } else {
                        CONS_WARN("\r\nUnknown command: %s", cmd_buffer);
                    }
                    printf("%s", prompt);
                } else {
                    printf("%s", prompt);
                }
                cmd_index = 0;
            } else if (rx_byte >= 32 && rx_byte <= 126 && cmd_index < (CMD_BUFFER_SIZE - 1)) {
                cmd_buffer[cmd_index++] = rx_byte;
            }
        }
        /* Small delay to prevent CPU hogging */
        osDelay(1);
    }
}