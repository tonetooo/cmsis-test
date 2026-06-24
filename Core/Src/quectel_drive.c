#include "quectel_drive.h"
#include "main.h"
#include "tasks.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "credentials.h"
#include "wdt.h"
#include "console.h"
#include "sd_spi.h"
#include "FreeRTOS.h"

// FatFs mount (re-mount before file access after long modem init)
extern FATFS fs;

#define MODEM_SIMULATION_ENABLED 0

static UART_HandleTypeDef *_modem_uart;
static char modem_rx_buffer[MODEM_BUFFER_SIZE];
static uint8_t modem_powered = 0;

/* ============================================================
 * Upload tracking via RTC backup registers (BKP0R..BKP19R)
 * Survives NRST, IWDG, software resets — no NAND program delay.
 * Bitmap: 1 bit per TRIG index.
 *   BKP0R bits  0-31 = TRIG_001..TRIG_032
 *   BKP1R bits  0-31 = TRIG_033..TRIG_064
 *   ...up to 19 regs × 32 = 608 TRIG indices
 * ============================================================ */

static uint8_t g_backup_inited = 0;

void Modem_BackupInit(void) {
    if (g_backup_inited) return;

    /* Enable RTC APB1 clock — needed to access backup register space */
    __HAL_RCC_RTC_ENABLE();

    /* Disable backup domain write protection (DBP) */
    HAL_PWR_EnableBkUpAccess();

    g_backup_inited = 1;
    CONS_DBG("[BKP] Backup register tracking initialized\r\n");
}

void Modem_MarkUploaded(int trig_idx) {
    if (trig_idx < 1) return;
    if (!g_backup_inited) Modem_BackupInit();

    int reg = (trig_idx - 1) / 32;
    int bit = (trig_idx - 1) % 32;
    if (reg >= 20) return; /* only BKP0R..BKP19R exist */

    __IO uint32_t* bkp_reg = (__IO uint32_t*)(RTC_BASE + 0x50U + (uint32_t)reg * 4U);
    *bkp_reg |= (1UL << bit);
    CONS_DBG("[BKP] Marked TRIG_%03d uploaded (reg=%d bit=%d)\r\n", trig_idx, reg, bit);
}

int Modem_IsUploaded(int trig_idx) {
    if (trig_idx < 1) return 0;

    int reg = (trig_idx - 1) / 32;
    int bit = (trig_idx - 1) % 32;
    if (reg >= 20) return 0;

    __IO uint32_t* bkp_reg = (__IO uint32_t*)(RTC_BASE + 0x50U + (uint32_t)reg * 4U);
    return (*bkp_reg & (1UL << bit)) ? 1 : 0;
}

/* Mark upload complete: backup register (instant, survives ANY reset) + .DONE file (best effort).
 * Called from Modem_UploadFile success paths BEFORE return HAL_OK. */
static void Modem_CreateDoneMarker(const char* filename) {
    int file_idx = 0;
    if (sscanf(filename, "TRIG_%d.CSV", &file_idx) == 1) {
        /* 1. Write to RTC backup register — survives NRST/IWDG, no NAND delay */
        Modem_MarkUploaded(file_idx);

        /* 2. Best-effort .DONE file on SD (filesystem consistency, human-readable) */
        char done_name[32];
        snprintf(done_name, sizeof(done_name), "TRIG_%03d.DONE", file_idx);
        osMutexAcquire(sd_mutexHandle, osWaitForever);
        FIL done_f;
        FRESULT done_fr = f_open(&done_f, done_name, FA_CREATE_NEW | FA_WRITE);
        if (done_fr == FR_OK) {
            f_close(&done_f); /* f_sync → disk_ioctl(CTRL_SYNC) → sd_wait_write_complete */
            /* Extra delay for SD card internal NAND program */
            HAL_Delay(50);
        }
        osMutexRelease(sd_mutexHandle);
    }
}

static HAL_StatusTypeDef Modem_BringUpNetwork(void) {
    CONS_INFO("[MODEM] Preparando red de datos...\r\n");

    // Re-sync con modem antes de SIM check (modem puede tardar ~30s en bootear)
    CONS_INFO("[MODEM] Esperando que modem responda AT...\r\n");
    uint8_t at_ready = 0;
    for (int sync_i = 0; sync_i < 15; sync_i++) {
        WDT_Refresh();
        HAL_Delay(1000);
        if (Modem_SendAT("AT", "OK", 1000) == HAL_OK) {
            CONS_OK("[MODEM] Re-sync OK (intento %d)\r\n", sync_i + 1);
            at_ready = 1;
            break;
        }
    }
    if (!at_ready) {
        CONS_ERR("[MODEM] Modem no responde AT tras 15s. Abortando red.\r\n");
        return HAL_ERROR;
    }
    // Esperar estabilizacion adicional
    HAL_Delay(2000);

    uint32_t sim_start = HAL_GetTick();
    while (HAL_GetTick() - sim_start < 20000) { // 20s timeout
        WDT_Refresh();
        // Re-sync si el modem dejo de responder
        Modem_SendAT("AT", "OK", 500);
        HAL_StatusTypeDef ret = Modem_SendAT("AT+CPIN?", "READY", 2000);
        if (ret == HAL_OK) {
            CONS_OK("[MODEM] SIM lista: %s\r\n", modem_rx_buffer);
            break;
        }
        // Si recibio +CME ERROR, mostrar codigo si existe
        if (strlen(modem_rx_buffer) > 0 && strstr(modem_rx_buffer, "ERROR")) {
            CONS_WARN("[MODEM] CPIN? -> %s\r\n", modem_rx_buffer);
            // Ver si es error de PIN (SIM bloqueada)
            if (strstr(modem_rx_buffer, "+CME ERROR: 13")) {
                CONS_WARN("[MODEM] -> SIM requiere PIN. Usa 'at AT+CPIN=1234' para desbloquear.\r\n");
            }
        }
        // Si la respuesta esta vacia, el modem aun bootea - esperar mas
        if (strlen(modem_rx_buffer) == 0) {
            CONS_DBG("[MODEM] CPIN? timeout - modem aun booteando, esperando 3s...\r\n");
            HAL_Delay(1000); // Delay extra ya incluido abajo
        }
        HAL_Delay(2000);
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",1", MODEM_APN, MODEM_APN_USER, MODEM_APN_PASS);
    if (Modem_SendAT(cmd, "OK", 10000) != HAL_OK) {
        CONS_ERR("[MODEM] Error configurando APN.\r\n");
        return HAL_ERROR;
    }

    CONS_INFO("[MODEM] Esperando registro en red...\r\n");
    uint32_t start = HAL_GetTick();
    uint8_t diag_counter = 0;
    while (HAL_GetTick() - start < 180000) { // Timeout aumentado a 3 min
        WDT_Refresh(); // Cada ~2s, evita WDT reset durante registro

        // Diagnostico de Estado de SIM y Operador (Cada 10s aprox)
        if (diag_counter++ % 5 == 0) {
             Modem_SendAT("AT+CPIN?", "+CPIN:", 1000);
             Modem_SendAT("AT+COPS?", "+COPS:", 1000);
             // Ver bandas configuradas por si acaso
             // Modem_SendAT("AT+QCFG=\"band\"", "OK", 1000); 
        }

        // Check Signal Quality con limpieza de buffer y parseo robusto
        memset(modem_rx_buffer, 0, MODEM_BUFFER_SIZE);
        if (Modem_SendAT("AT+CSQ", "+CSQ:", 1000) == HAL_OK) {
             // Debug: printf("[MODEM] RAW CSQ: %s\r\n", modem_rx_buffer);
             char* p = strstr(modem_rx_buffer, "+CSQ:");
             if (p) {
                 int rssi = 99, ber = 99;
                 p += 5; // Saltar "+CSQ:"
                 while(*p == ' ') p++; // Saltar espacios
                 
                 // Robust parsing: skip any non-numeric garbage before RSSI
                 while(*p && !isdigit((unsigned char)*p) && *p != '-') p++;
                 
                 if (sscanf(p, "%d,%d", &rssi, &ber) >= 1) {
                     int dbm = (rssi >= 0 && rssi <= 31) ? rssi * 2 - 110 : -999;
                     int bars = (rssi >= 20) ? 5 : (rssi >= 15) ? 4 : (rssi >= 10) ? 3 : (rssi >= 5) ? 2 : (rssi > 0) ? 1 : 0;
                     const char *qlabel = (bars >= 4) ? "EXCELENTE" : (bars == 3) ? "BUENA" : (bars == 2) ? "MEDIA" : (bars == 1) ? "BAJA" : "SIN SIGNAL";
                     const char *ber_txt = (ber == 0) ? "<0.2%" : (ber == 1) ? "0.2-0.4%" : (ber == 2) ? "0.4-0.8%" : (ber <= 4) ? "0.8-3.2%" : (ber <= 7) ? ">3.2%" : "DESCONOCIDO";
                     CONS_INFO("[MODEM] Señal: %s (%ddBm, %d/5 barras) | BER: %s\r\n", qlabel, dbm, bars, ber_txt);
                 } else {
                     char* alt = modem_rx_buffer;
                     while(*alt && !isdigit((unsigned char)*alt)) alt++;
                     if (sscanf(alt, "%d,%d", &rssi, &ber) >= 1) {
                         int dbm = (rssi >= 0 && rssi <= 31) ? rssi * 2 - 110 : -999;
                         int bars = (rssi >= 20) ? 5 : (rssi >= 15) ? 4 : (rssi >= 10) ? 3 : (rssi >= 5) ? 2 : (rssi > 0) ? 1 : 0;
                         const char *qlabel = (bars >= 4) ? "EXCELENTE" : (bars == 3) ? "BUENA" : (bars == 2) ? "MEDIA" : (bars == 1) ? "BAJA" : "SIN SIGNAL";
                         const char *ber_txt = (ber == 0) ? "<0.2%" : (ber == 1) ? "0.2-0.4%" : (ber == 2) ? "0.4-0.8%" : (ber <= 4) ? "0.8-3.2%" : (ber <= 7) ? ">3.2%" : "DESCONOCIDO";
                         CONS_INFO("[MODEM] Señal: %s (%ddBm, %d/5 barras) | BER: %s\r\n", qlabel, dbm, bars, ber_txt);
                     } else {
                         CONS_WARN("[MODEM] Signal Quality Parse Error. RAW: %s\r\n", modem_rx_buffer);
                     }
                 }
             } else {
                 char* alt = modem_rx_buffer;
                 while(*alt && !isdigit((unsigned char)*alt)) alt++;
                 int rssi = 99, ber = 99;
                 if (sscanf(alt, "%d,%d", &rssi, &ber) >= 1) {
                     int dbm = (rssi >= 0 && rssi <= 31) ? rssi * 2 - 110 : -999;
                     int bars = (rssi >= 20) ? 5 : (rssi >= 15) ? 4 : (rssi >= 10) ? 3 : (rssi >= 5) ? 2 : (rssi > 0) ? 1 : 0;
                     const char *qlabel = (bars >= 4) ? "EXCELENTE" : (bars == 3) ? "BUENA" : (bars == 2) ? "MEDIA" : (bars == 1) ? "BAJA" : "SIN SIGNAL";
                     const char *ber_txt = (ber == 0) ? "<0.2%" : (ber == 1) ? "0.2-0.4%" : (ber == 2) ? "0.4-0.8%" : (ber <= 4) ? "0.8-3.2%" : (ber <= 7) ? ">3.2%" : "DESCONOCIDO";
                     CONS_INFO("[MODEM] Señal: %s (%ddBm, %d/5 barras) | BER: %s\r\n", qlabel, dbm, bars, ber_txt);
                 } else {
                     CONS_WARN("[MODEM] Signal Quality Parse Error. RAW: %s\r\n", modem_rx_buffer);
                 }
             }
        }

        if (Modem_SendAT("AT+CEREG?", "+CEREG: 0,1", 2000) == HAL_OK ||
            Modem_SendAT("AT+CEREG?", "+CEREG: 0,5", 2000) == HAL_OK ||
            Modem_SendAT("AT+CREG?", "+CREG: 0,1", 2000) == HAL_OK  ||
            Modem_SendAT("AT+CREG?", "+CREG: 0,5", 2000) == HAL_OK) {
            CONS_OK("[MODEM] Registrado en red movil.\r\n");
            break;
        }
        
        // Si recibimos "0,3" (Denied), intentar recuperación
        if (strstr(modem_rx_buffer, ": 0,3")) {
             CONS_WARN("[MODEM] Registro DENEGADO (0,3). Intentando reinicio de RF...\r\n");
             Modem_SendAT("AT+CFUN=0", "OK", 5000); // Modo avion
             HAL_Delay(2000);
             Modem_SendAT("AT+CFUN=1", "OK", 5000); // Modo normal
             HAL_Delay(5000); // Esperar a que inicie RF
        }
        
        HAL_Delay(2000);
    }

    if (HAL_GetTick() - start >= 180000) {
        CONS_ERR("[MODEM] Tiempo de registro agotado (3 min).\r\n");
        return HAL_ERROR;
    }

    if (Modem_SendAT("AT+QIACT=1", "OK", 60000) != HAL_OK) {
        CONS_ERR("[MODEM] Error activando PDP.\r\n");
        return HAL_ERROR;
    }

    CONS_OK("[MODEM] PDP activo.\r\n");

    HAL_Delay(8000);
    WDT_Refresh();

    /* Verify PDP is actually active and DNS is ready */
    if (Modem_SendAT("AT+CGACT?", "1,1", 10000) != HAL_OK) {
        CONS_WARN("[MODEM] PDP context not active, attempting re-activation...");
        Modem_SendAT("AT+QIACT=1", "OK", 60000);
        HAL_Delay(8000);
        WDT_Refresh();
    }

    CONS_INFO("[MODEM] Sincronizando hora via NTP...\r\n");
    if (Modem_SendAT("AT+QNTP=\"time.google.com\",1", "OK", 60000) == HAL_OK) {
        CONS_OK("[MODEM] NTP sync completed\r\n");
    } else {
        CONS_WARN("[MODEM] NTP primary failed, trying pool.ntp.org...\r\n");
        if (Modem_SendAT("AT+QNTP=\"pool.ntp.org\",1", "OK", 60000) == HAL_OK) {
            CONS_OK("[MODEM] NTP sync completed (fallback)\r\n");
        } else {
            CONS_WARN("[MODEM] NTP sync failed, using system time\r\n");
        }
    }

    WDT_Refresh();
    return HAL_OK;
}

void Modem_Init(UART_HandleTypeDef *huart) {
    _modem_uart = huart;
}

HAL_StatusTypeDef Modem_PowerOn(void) {
    /* NULL guard: if Modem_Init was never called, don't crash */
    if (_modem_uart == NULL) {
        CONS_ERR("[MODEM] ERROR: _modem_uart is NULL (Modem_Init not called)\r\n");
        return HAL_ERROR;
    }
#if MODEM_SIMULATION_ENABLED
    CONS_INFO("[MODEM-SIM] PowerOn (simulated)\r\n");
    return HAL_OK;
#else
    /* Fast path: modem was previously powered, just wake it */
    if (modem_powered) {
        WDT_Refresh();
        if (Modem_SendAT("AT", "OK", 2000) == HAL_OK) {
            CONS_DBG("[MODEM] Already powered, AT responsive\r\n");
            return HAL_OK;
        }
        CONS_WARN("[MODEM] AT silent, trying CFUN=1 to wake...\r\n");
        if (Modem_SendAT("AT+CFUN=1", "OK", 10000) == HAL_OK) {
            return HAL_OK;
        }
        CONS_WARN("[MODEM] Fast wake failed, full re-init...\r\n");
        modem_powered = 0;
    }

    /* Try AT first — modem may be alive from previous boot (HAT stays on after NRST glitch fix) */
    WDT_Refresh();
    if (Modem_SendAT("AT", "OK", 2000) == HAL_OK) {
        CONS_INFO("[MODEM] Modem already alive (AT OK) — skipping PWRKEY pulse\r\n");
        modem_powered = 1;
        return HAL_OK;
    }
    /* Modem might be in sleep/minimum functionality — try CFUN=1 */
    if (Modem_SendAT("AT+CFUN=1", "OK", 10000) == HAL_OK) {
        CONS_INFO("[MODEM] Modem woke from CFUN=0 — skipping PWRKEY pulse\r\n");
        modem_powered = 1;
        return HAL_OK;
    }

    /* Modem truly off — pulse PWRKEY (no HAT_PWR_OFF toggle — avoid NRST glitch) */
    CONS_INFO("[MODEM] AT silent, pulsing PWRKEY (2s)...\r\n");
    HAL_GPIO_WritePin(MODEM_PWRKEY_GPIO_Port, MODEM_PWRKEY_Pin, GPIO_PIN_SET);
    HAL_Delay(2100);
    HAL_GPIO_WritePin(MODEM_PWRKEY_GPIO_Port, MODEM_PWRKEY_Pin, GPIO_PIN_RESET);

    CONS_INFO("[MODEM] Esperando inicio de firmware y RDY (Max 30s)...\r\n");

    uint32_t start_firmware = HAL_GetTick();
    uint32_t null_count = 0;
    uint32_t total_bytes = 0;
    char rdy_buf[3] = {0};
    uint8_t rdy_idx = 0;
    uint8_t rdy_printed = 0;
    uint32_t last_heartbeat = 0;

    while(HAL_GetTick() - start_firmware < 30000) {
        WDT_Refresh();
        if (g_modem_abort_enabled && (osEventFlagsGet(sensor_event_flagsHandle) & EVT_MOTION_DETECTED)) {
            CONS_WARN("[MODEM] Abortando PowerOn por evento.\r\n");
            return HAL_BUSY;
        }
        uint32_t elapsed = HAL_GetTick() - start_firmware;
        if (elapsed - last_heartbeat >= 5000) {
            CONS_DBG("[MODEM][DIAG] Heartbeat t=%lu ms, bytes_recibidos=%lu\r\n",
                   (unsigned long)elapsed, (unsigned long)total_bytes);
            last_heartbeat = elapsed;
        }
        uint8_t byte;
        __HAL_UART_CLEAR_FLAG(_modem_uart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
        if(HAL_UART_Receive(_modem_uart, &byte, 1, 10) == HAL_OK) {
            total_bytes++;
            if (byte == 0x00) { null_count++; continue; }
            null_count = 0;
            CONS_DBG("[MODEM][RX] Byte=0x%02X ('%c')\r\n", byte, (byte>=32 && byte<=126) ? byte : '.');
            if (!rdy_printed) {
                rdy_buf[rdy_idx % 3] = (char)byte;
                rdy_idx++;
                if (rdy_buf[(rdy_idx-3)%3] == 'R' && rdy_buf[(rdy_idx-2)%3] == 'D' && rdy_buf[(rdy_idx-1)%3] == 'Y') {
                    CONS_OK("[MODEM] RDY recibido\r\n");
                    rdy_printed = 1;
                    break;
                }
            }
        }
    }
    CONS_DBG("[MODEM][DIAG] Total bytes: %lu, Null: %lu\r\n",
           (unsigned long)total_bytes, (unsigned long)null_count);

    if (total_bytes == 0) {
        CONS_ERR("[MODEM] CERO bytes. Modem no enciende.\r\n");
        // Fallback: second PWRKEY pulse directly
        CONS_DBG("[MODEM][DIAG] Fallback: second PWRKEY pulse...\r\n");
        HAL_GPIO_WritePin(MODEM_PWRKEY_GPIO_Port, MODEM_PWRKEY_Pin, GPIO_PIN_SET);
        HAL_Delay(2000);
        HAL_GPIO_WritePin(MODEM_PWRKEY_GPIO_Port, MODEM_PWRKEY_Pin, GPIO_PIN_RESET);

        total_bytes = 0; null_count = 0; rdy_printed = 0; rdy_idx = 0;
        memset(rdy_buf, 0, sizeof(rdy_buf)); last_heartbeat = 0;
        uint32_t fb_start = HAL_GetTick();
        while(HAL_GetTick() - fb_start < 20000) {
            WDT_Refresh();
            if (g_modem_abort_enabled && (osEventFlagsGet(sensor_event_flagsHandle) & EVT_MOTION_DETECTED)) return HAL_BUSY;
            uint32_t fb_elapsed = HAL_GetTick() - fb_start;
            if (fb_elapsed - last_heartbeat >= 5000) {
                CONS_DBG("[MODEM][DIAG] FB Heartbeat t=%lu ms, bytes=%lu\r\n",
                       (unsigned long)fb_elapsed, (unsigned long)total_bytes);
                last_heartbeat = fb_elapsed;
            }
            uint8_t byte;
            __HAL_UART_CLEAR_FLAG(_modem_uart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
            if(HAL_UART_Receive(_modem_uart, &byte, 1, 10) == HAL_OK) {
                total_bytes++;
                if (byte == 0x00) { null_count++; continue; }
                null_count = 0;
                if (!rdy_printed) {
                    rdy_buf[rdy_idx % 3] = (char)byte; rdy_idx++;
                    if (rdy_buf[(rdy_idx-3)%3] == 'R' && rdy_buf[(rdy_idx-2)%3] == 'D' && rdy_buf[(rdy_idx-1)%3] == 'Y') {
                        CONS_OK("[MODEM] RDY recibido\r\n"); rdy_printed = 1; break;
                    }
                }
            }
        }
        if (total_bytes == 0) {
            CONS_ERR("[MODEM] Modem no enciende. Verificar: fuente 2A+, TX/RX cruzados.\r\n");
            return HAL_ERROR;
        }
    }

    // Sincronizacion AT
    CONS_INFO("[MODEM] Sincronizando baudrate...\r\n");
    for(int i=0; i<30; i++) {
        WDT_Refresh();
        if (g_modem_abort_enabled && (osEventFlagsGet(sensor_event_flagsHandle) & EVT_MOTION_DETECTED)) return HAL_BUSY;
        uint8_t d;
        while(HAL_UART_Receive(_modem_uart, &d, 1, 0) == HAL_OK);
        __HAL_UART_CLEAR_FLAG(_modem_uart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
        if (Modem_SendAT("AT", "OK", 1000) == HAL_OK) {
            CONS_OK("[MODEM] Comunicacion establecida\r\n");
            Modem_SendAT("ATE0", "OK", 1000);
            modem_powered = 1;
            return HAL_OK;
        }
        HAL_Delay(500);
    }
    CONS_ERR("[MODEM] Error de comunicacion inicial.\r\n");
    return HAL_ERROR;
#endif
}

void Modem_PowerOff(void) {
    if (!modem_powered) {
        CONS_DBG("[MODEM] Already off, skipping PowerOff\r\n");
        return;
    }
    CONS_WARN("[MODEM] PowerOff (software only, no HAT toggle)\r\n");
    WDT_Refresh();
    // Software shutdown only — NO HAT_PWR_OFF toggle (avoids NRST glitch)
    if (Modem_SendAT("AT+QPOWD=1", "POWERED DOWN", 5000) != HAL_OK) {
        CONS_WARN("[MODEM] SW poweroff failed.\r\n");
    }
    WDT_Refresh();
    HAL_Delay(500);
    WDT_Refresh();
    modem_powered = 0;
    CONS_OK("[MODEM] Modem shut down (HAT powered).\r\n");
}

void Modem_Sleep(void) {
    if (!modem_powered) {
        CONS_DBG("[MODEM] Not powered, skipping Sleep\r\n");
        return;
    }
    CONS_INFO("[MODEM] Entering sleep mode (CFUN=0)...\r\n");
    WDT_Refresh();
    Modem_SendAT("AT+CFUN=0", "OK", 5000);
    WDT_Refresh();
    CONS_OK("[MODEM] Modem asleep (CFUN=0).\r\n");
    /* Note: modem_powered stays 1 — modem is still physically on */
}

HAL_StatusTypeDef Modem_SendAT(char* command, char* expected_reply, uint32_t timeout) {
#if MODEM_SIMULATION_ENABLED
    CONS_DBG("[MODEM-SIM] AT: %s -> expect: %s\r\n", command, expected_reply);
    if (strstr(command, "+CSQ") != NULL) {
        CONS_DBG("[MODEM-SIM] RX: +CSQ: 20,0\r\n");
    } else if (strstr(command, "+CPIN?") != NULL) {
        CONS_DBG("[MODEM-SIM] RX: +CPIN: READY\r\n");
    } else if (strstr(command, "CEREG?") != NULL || strstr(command, "CREG?") != NULL) {
        CONS_DBG("[MODEM-SIM] RX: +CEREG: 0,1\r\n");
    } else if (strstr(command, "QIACT") != NULL) {
        CONS_DBG("[MODEM-SIM] RX: OK (PDP active)\r\n");
    } else if (strstr(command, "QHTTPURL") != NULL) {
        CONS_DBG("[MODEM-SIM] RX: CONNECT\r\n");
    } else if (strstr(command, "QHTTPPOST") != NULL) {
        CONS_DBG("[MODEM-SIM] RX: CONNECT\r\n");
    } else if (strstr(command, "QHTTPREAD") != NULL) {
        CONS_DBG("[MODEM-SIM] RX: +QHTTPPOST: 0,200,0\r\n");
    } else if (strstr(command, "QHTTPCFG") != NULL || strstr(command, "QSSLCFG") != NULL) {
        CONS_DBG("[MODEM-SIM] RX: OK\r\n");
    } else {
        CONS_DBG("[MODEM-SIM] RX: %s\r\n", expected_reply);
    }
    snprintf(modem_rx_buffer, sizeof(modem_rx_buffer), "%s", expected_reply);
    HAL_Delay(20);
    return HAL_OK;
#else
    if (_modem_uart == NULL) {
        CONS_ERR("[MODEM] UART not initialized (Modem_Init never called)\r\n");
        return HAL_ERROR;
    }
    char full_cmd[128];
    snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", command);
    
    memset(modem_rx_buffer, 0, MODEM_BUFFER_SIZE);
    
    // 1. Limpiar buffer de entrada UART y errores antes de enviar
    uint8_t dummy;
    while(HAL_UART_Receive(_modem_uart, &dummy, 1, 0) == HAL_OK);
    __HAL_UART_CLEAR_FLAG(_modem_uart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);

    // 2. Transmitir comando
    HAL_UART_Transmit(_modem_uart, (uint8_t*)full_cmd, strlen(full_cmd), 1000);
    
    // 3. Recepción con timeout
    uint32_t start_tick = HAL_GetTick();
    uint16_t idx = 0;
    
    while ((HAL_GetTick() - start_tick) < timeout) {
        WDT_Refresh(); // Refresh c/iteración (~50ms), evita WDT reset en timeouts largos (60s+)
        if (g_modem_abort_enabled && (osEventFlagsGet(sensor_event_flagsHandle) & EVT_MOTION_DETECTED)) {
            CONS_WARN("[MODEM] Aborted by motion event.\r\n");
            return HAL_ERROR;
        }
        uint8_t byte;
        if (HAL_UART_Receive(_modem_uart, &byte, 1, 50) == HAL_OK) {
            if (idx < MODEM_BUFFER_SIZE - 1) {
                modem_rx_buffer[idx++] = byte;
                modem_rx_buffer[idx] = '\0';
            }
        }
        
        if (strstr(modem_rx_buffer, expected_reply) != NULL) {
            return HAL_OK;
        }
        
        if (strstr(modem_rx_buffer, "ERROR") != NULL) {
            CONS_ERR("[MODEM] AT ERR | %s | Resp=%s\r\n", command, modem_rx_buffer);
            return HAL_ERROR;
        }

        if (__HAL_UART_GET_FLAG(_modem_uart, UART_FLAG_ORE)) {
            __HAL_UART_CLEAR_FLAG(_modem_uart, UART_FLAG_ORE);
        }
    }
    CONS_ERR("[MODEM] AT TIMEOUT | %s | Resp=%s\r\n", command, modem_rx_buffer);
    return HAL_TIMEOUT;
#endif
}

HAL_StatusTypeDef Modem_WaitFor(const char* expected, uint32_t timeout) {
    uint32_t start = HAL_GetTick();
    char buf[128];
    uint16_t idx = 0;
    
    // Limpiar buffer
    memset(buf, 0, sizeof(buf));

    while (HAL_GetTick() - start < timeout) {
        WDT_Refresh(); // Refresh c/iteración (~5ms), evita WDT reset en waits de 120s
        uint8_t c;
        // CRITICAL FIX: Limpiar ORE (Overrun) si ocurre
        if (__HAL_UART_GET_FLAG(_modem_uart, UART_FLAG_ORE)) {
            __HAL_UART_CLEAR_OREFLAG(_modem_uart);
        }

        // Timeout reducido para lectura rapida
        if (HAL_UART_Receive(_modem_uart, &c, 1, 5) == HAL_OK) {
            if (idx < sizeof(buf) - 1) {
                buf[idx++] = (char)c;
                buf[idx] = 0;
            } else {
                // Buffer circular simple: mover todo 1 a la izquierda
                memmove(buf, buf + 1, sizeof(buf) - 2);
                buf[sizeof(buf) - 2] = (char)c;
                buf[sizeof(buf) - 1] = 0;
                idx = sizeof(buf) - 1;
            }
            
            // Debug opcional: putchar(c);
            
            if (strstr(buf, expected) != NULL) {
                CONS_DBG("[MODEM] WaitFor '%s' FOUND\r\n", expected);
                return HAL_OK;
            }
            
            if (strstr(buf, "ERROR") != NULL) {
                CONS_ERR("[MODEM] WaitFor '%s' ERROR\r\n", expected);
                return HAL_ERROR;
            }
        }
    }
    CONS_ERR("[MODEM] WaitFor TIMEOUT | '%s' | Resp=%s\r\n", expected, buf);
    return HAL_TIMEOUT;
}

HAL_StatusTypeDef Modem_CheckConnection(void) {
    CONS_INFO("[MODEM] Checking network registration...\r\n");
    if (Modem_SendAT("AT+CREG?", "+CREG: 0,1", 2000) == HAL_OK || 
        Modem_SendAT("AT+CREG?", "+CREG: 0,5", 2000) == HAL_OK) {
        CONS_OK("[MODEM] Registered on network.\r\n");
        return HAL_OK;
    }
    CONS_WARN("[MODEM] Not registered.\r\n");
    return HAL_ERROR;
}

HAL_StatusTypeDef Modem_UploadFile(const char* filename) {
    CONS_INFO("[MODEM] Uploading %s...\r\n", filename);

#if MODEM_SIMULATION_ENABLED
    CONS_DBG("[MODEM-SIM] PowerOn -> OK\r\n");
    CONS_DBG("[MODEM-SIM] BringUpNetwork -> OK\r\n");
    CONS_DBG("[MODEM-SIM] Opening %s\r\n", filename);
    FIL sf;
    if (f_open(&sf, filename, FA_READ) == FR_OK) {
        DWORD sz = f_size(&sf);
        CONS_DBG("[MODEM-SIM] File size: %lu bytes\r\n", (unsigned long)sz);
        CONS_DBG("[MODEM-SIM] URL: %s?filename=%s&key=%s\r\n", BACKEND_UPLOAD_URL, filename, BACKEND_API_KEY);
        CONS_DBG("[MODEM-SIM] Connecting...\r\n");
        CONS_DBG("[MODEM-SIM] Sending HTTP headers...\r\n");
        CONS_DBG("[MODEM-SIM] Sending %lu bytes...\r\n", (unsigned long)sz);
        CONS_DBG("[MODEM-SIM] Waiting for response...\r\n");
        CONS_DBG("[MODEM-SIM] Response: 200 OK (simulated)\r\n");
        f_close(&sf);
    } else {
        CONS_ERR("[MODEM-SIM] Could not open %s\r\n", filename);
        return HAL_ERROR;
    }
    CONS_DBG("[MODEM-SIM] Upload complete.\r\n");
    return HAL_OK;
#else
    if (Modem_PowerOn() != HAL_OK) return HAL_ERROR;
    if (Modem_BringUpNetwork() != HAL_OK) {
        Modem_PowerOff();
        return HAL_ERROR;
    }

    Modem_SendAT("AT+QHTTPCFG=\"contextid\",1", "OK", 1000);
    Modem_SendAT("AT+QHTTPCFG=\"requestheader\",0", "OK", 1000);
    Modem_SendAT("AT+QHTTPCFG=\"responseheader\",0", "OK", 1000);
    Modem_SendAT("AT+QSSLCFG=\"sslversion\",1,4", "OK", 1000);
    Modem_SendAT("AT+QSSLCFG=\"seclevel\",1,0", "OK", 1000);
    Modem_SendAT("AT+QHTTPCFG=\"sslctxid\",1", "OK", 1000);

    /* Ruta 1: Google Drive directo si hay token y folder ID */
    if (GDRIVE_TOKEN[0] != 0 && GDRIVE_FOLDER_ID[0] != 0) {
        const char* url = "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart";
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "AT+QHTTPURL=%d,30", (int)strlen(url));
        if (Modem_SendAT(cmd, "CONNECT", 2000) != HAL_OK) { Modem_PowerOff(); return HAL_ERROR; }
        HAL_UART_Transmit(_modem_uart, (uint8_t*)url, strlen(url), 2000);
        Modem_SendAT("", "OK", 2000);

        char auth_hdr[256];
        snprintf(auth_hdr, sizeof(auth_hdr), "AT+QHTTPHDR=\"Authorization: Bearer %s\"", GDRIVE_TOKEN);
        if (Modem_SendAT(auth_hdr, "OK", 2000) != HAL_OK) { Modem_PowerOff(); return HAL_ERROR; }

        const char* boundary = "----AWTASBOUNDARY";
        char ct_hdr[128];
        snprintf(ct_hdr, sizeof(ct_hdr), "AT+QHTTPHDR=\"Content-Type: multipart/related; boundary=%s\"", boundary);
        if (Modem_SendAT(ct_hdr, "OK", 2000) != HAL_OK) { Modem_PowerOff(); return HAL_ERROR; }

        char fullpath[32];
        snprintf(fullpath, sizeof(fullpath), "0:%s", filename);
        FIL f;
        osMutexAcquire(sd_mutexHandle, osWaitForever);
        FRESULT fr = f_open(&f, fullpath, FA_READ);
        if (fr != FR_OK) {
            CONS_WARN("[MODEM] Drive f_open failed (FR=%d), remounting...\r\n", (int)fr);
            f_mount(NULL, "0:", 0);
            osDelay(50);
            f_mount(&fs, "0:", 1);
            fr = f_open(&f, fullpath, FA_READ);
        }
        if (fr != FR_OK) {
            CONS_WARN("[MODEM] Remount failed (FR=%d), remounting with correct path...\r\n", (int)fr);
            f_mount(NULL, "0:", 0);
            osDelay(50);
            f_mount(&fs, "0:/", 1);  /* use same path as boot: "0:/" */
            fr = f_open(&f, fullpath, FA_READ);
        }
        if (fr != FR_OK) { osMutexRelease(sd_mutexHandle); Modem_PowerOff(); return HAL_ERROR; }
        DWORD fsz = f_size(&f);

        char meta[256];
        snprintf(meta, sizeof(meta), "{\"name\":\"%s\",\"parents\":[\"%s\"]}", filename, GDRIVE_FOLDER_ID);

        char pre[512];
        int pre_len = snprintf(pre, sizeof(pre),
            "--%s\r\n"
            "Content-Type: application/json; charset=UTF-8\r\n\r\n"
            "%s\r\n"
            "--%s\r\n"
            "Content-Type: text/csv\r\n\r\n",
            boundary, meta, boundary);
        const char* post_fmt = "\r\n--%s--\r\n";
        char post[64];
        int post_len = snprintf(post, sizeof(post), post_fmt, boundary);
        uint32_t total_len = (uint32_t)pre_len + fsz + (uint32_t)post_len;

        snprintf(cmd, sizeof(cmd), "AT+QHTTPPOST=%lu,60", (unsigned long)total_len);
        if (Modem_SendAT(cmd, "CONNECT", 5000) != HAL_OK) { f_close(&f); osMutexRelease(sd_mutexHandle); Modem_PowerOff(); return HAL_ERROR; }

        HAL_UART_Transmit(_modem_uart, (uint8_t*)pre, pre_len, 5000);

        UINT br;
        uint8_t buf[HTTP_CHUNK_SIZE];
        DWORD remaining = fsz;
        while (remaining > 0) {
            UINT to_read = remaining > HTTP_CHUNK_SIZE ? HTTP_CHUNK_SIZE : (UINT)remaining;
            if (f_read(&f, buf, to_read, &br) != FR_OK) { f_close(&f); osMutexRelease(sd_mutexHandle); Modem_PowerOff(); return HAL_ERROR; }
            if (br == 0) break;
            HAL_UART_Transmit(_modem_uart, buf, br, 5000);
            remaining -= br;
        }

        HAL_UART_Transmit(_modem_uart, (uint8_t*)post, post_len, 5000);
        f_close(&f);
        osMutexRelease(sd_mutexHandle);

        if (Modem_SendAT("AT+QHTTPREAD=60", "OK", 6000) != HAL_OK) { Modem_PowerOff(); return HAL_ERROR; }
        Modem_CreateDoneMarker(filename);
        CONS_OK("[MODEM] Upload complete (Drive).\r\n");
        /* NOTA: Modem_PowerOff() se llama en modem_task DESPUÉS de
         * upload_queue_pop() + creación de .DONE para evitar NRST
         * antes de que el queue se actualice. */
        return HAL_OK;
    }

    /* Ruta 2: Backend propio si hay URL configurada */
    if (BACKEND_UPLOAD_URL[0] != 0) {
        char url[256];
        if (BACKEND_API_KEY[0] != 0) {
            snprintf(url, sizeof(url), "%s?filename=%s&key=%s", BACKEND_UPLOAD_URL, filename, BACKEND_API_KEY);
        } else {
            snprintf(url, sizeof(url), "%s?filename=%s", BACKEND_UPLOAD_URL, filename);
        }

        // Parsear URL para Host y Path (necesario para headers manuales)
        char host[128] = {0};
        char path[256] = {0};
        char* p_proto = strstr(url, "://");
        char* p_host = p_proto ? p_proto + 3 : url;
        char* p_path = strchr(p_host, '/');
        
        if (p_path) {
            size_t host_len = p_path - p_host;
            if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
            strncpy(host, p_host, host_len);
            strncpy(path, p_path, sizeof(path) - 1);
        } else {
            strncpy(host, p_host, sizeof(host) - 1);
            strcpy(path, "/");
        }

        // Configuracion SSL y HTTP explicita justo antes del uso
        Modem_SendAT("AT+QHTTPCFG=\"contextid\",1", "OK", 1000);
        Modem_SendAT("AT+QHTTPCFG=\"requestheader\",1", "OK", 1000); // Cabeceras manuales
        Modem_SendAT("AT+QHTTPCFG=\"responseheader\",1", "OK", 1000); // Ver cabeceras respuesta
        Modem_SendAT("AT+QSSLCFG=\"sslversion\",1,4", "OK", 1000);
        Modem_SendAT("AT+QSSLCFG=\"seclevel\",1,0", "OK", 1000);
        Modem_SendAT("AT+QHTTPCFG=\"sslctxid\",1", "OK", 1000);

        char cmd[64];
        // Timeout URL aumentado a 60s
        snprintf(cmd, sizeof(cmd), "AT+QHTTPURL=%d,60", (int)strlen(url));
        if (Modem_SendAT(cmd, "CONNECT", 2000) != HAL_OK) {
            Modem_PowerOff();
            return HAL_ERROR;
        }

        CONS_DBG("[MODEM] URL: %s\r\n", url);
        HAL_UART_Transmit(_modem_uart, (uint8_t*)url, strlen(url), 2000);
        Modem_SendAT("", "OK", 60000); // Esperar OK de URL hasta 60s
        WDT_Refresh(); // Refresh tras espera larga de URL

        char fullpath[32];
        snprintf(fullpath, sizeof(fullpath), "0:%s", filename);

        /* === DETERMINE DATA SOURCE (SD CSV or RAM binary-to-CSV) === */
        FIL f;
        int use_sd = 0;
        uint32_t datasize = 0;
        /* Try SD card first — NO sd_init() (destroys FatFs state!) */
        osMutexAcquire(sd_mutexHandle, osWaitForever);
        CONS_DBG("[MODEM] f_open CSV from SD: %s\r\n", fullpath);
        FRESULT fr = f_open(&f, fullpath, FA_READ);
        if (fr != FR_OK) {
            /* Step 1: simple remount (no HW reinit) */
            CONS_WARN("[MODEM] SD f_open failed (FR=%d), remounting FatFs...\r\n", (int)fr);
            f_mount(NULL, "0:", 0);   /* unmount */
            osDelay(50);
            f_mount(&fs, "0:/", 1);    /* force remount - use same path as boot */

            /* DIAG: Check filesystem info after remount */
            {
                FATFS* diag_fs;
                DWORD diag_free;
                FRESULT diag_fr = f_getfree("0:/", &diag_free, &diag_fs);
                if (diag_fr == FR_OK) {
                    DWORD tot = (diag_fs->n_fatent - 2) * diag_fs->csize;
                    DWORD fre = diag_free * diag_fs->csize;
                    CONS_WARN("[MODEM] FS after remount: ~%lu MB total, ~%lu MB free",
                              (unsigned long)(tot / 2048), (unsigned long)(fre / 2048));
                } else {
                    CONS_ERR("[MODEM] f_getfree after remount FAILED (FR=%d)", diag_fr);
                }
            }

            fr = f_open(&f, fullpath, FA_READ);
        }
        if (fr != FR_OK) {
            /* Step 2: remount with correct path (no HW reinit, no driver re-link) */
            CONS_WARN("[MODEM] Remount failed (FR=%d), remounting with correct path...\r\n", (int)fr);
            f_mount(NULL, "0:", 0);
            osDelay(50);
            f_mount(&fs, "0:/", 1);  /* use same path as boot: "0:/" */

            /* DIAG: Check filesystem info after second remount */
            {
                FATFS* diag_fs;
                DWORD diag_free;
                FRESULT diag_fr = f_getfree("0:/", &diag_free, &diag_fs);
                if (diag_fr == FR_OK) {
                    DWORD tot = (diag_fs->n_fatent - 2) * diag_fs->csize;
                    DWORD fre = diag_free * diag_fs->csize;
                    CONS_WARN("[MODEM] FS after second remount: ~%lu MB total, ~%lu MB free",
                              (unsigned long)(tot / 2048), (unsigned long)(fre / 2048));
                } else {
                    CONS_ERR("[MODEM] f_getfree after second remount FAILED (FR=%d)", diag_fr);
                }
            }

            fr = f_open(&f, fullpath, FA_READ);
        }
        if (fr == FR_OK) {
            datasize = f_size(&f);
            f_close(&f);
            use_sd = 1;
            CONS_OK("[MODEM] CSV found on SD (%lu bytes)\r\n", (unsigned long)datasize);
            osMutexRelease(sd_mutexHandle);
        } else {
            /* === DIAG: List root directory to see what files exist after remount === */
            {
                DIR diag_dir;
                FILINFO diag_fno;
                FRESULT dir_fr = f_opendir(&diag_dir, "0:/");
                if (dir_fr == FR_OK) {
                    CONS_WARN("[MODEM] DIR listing after remount (FR=%d):", fr);
                    int file_count = 0;
                    while (f_readdir(&diag_dir, &diag_fno) == FR_OK && diag_fno.fname[0]) {
                        CONS_WARN("[MODEM]   %-20s %10lu bytes", diag_fno.fname, (unsigned long)diag_fno.fsize);
                        file_count++;
                    }
                    CONS_WARN("[MODEM]   -- %d files total --", file_count);
                    f_closedir(&diag_dir);
                } else {
                    CONS_ERR("[MODEM] DIR listing failed after remount (FR=%d)", dir_fr);
                }
            }
            osMutexRelease(sd_mutexHandle);
            CONS_ERR("[MODEM] CSV '%s' not found on SD — upload failed\r\n", fullpath);
            Modem_PowerOff();
            return HAL_ERROR;
        }

        // CSV content type
        const char *content_type = "text/csv";
        char header[640];
        int header_len;
        if (BACKEND_API_KEY[0] != 0) {
            header_len = snprintf(header, sizeof(header),
                "POST %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "X-Api-Key: %s\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %lu\r\n"
                "\r\n",
                path, host, BACKEND_API_KEY, content_type, (unsigned long)datasize);
        } else {
            header_len = snprintf(header, sizeof(header),
                "POST %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %lu\r\n"
                "\r\n",
                path, host, content_type, (unsigned long)datasize);
        }
            
        uint32_t total_len = header_len + datasize;

        // Timeout POST aumentado a 120s, wait time 120s
        snprintf(cmd, sizeof(cmd), "AT+QHTTPPOST=%lu,120,120", (unsigned long)total_len);
        if (Modem_SendAT(cmd, "CONNECT", 15000) != HAL_OK) {
            if (use_sd) { /* nothing to close */ }
            Modem_PowerOff();
            return HAL_ERROR;
        }

        // Enviar Cabeceras
        HAL_UART_Transmit(_modem_uart, (uint8_t*)header, header_len, 2000);

        // === SEND DATA ===
        if (use_sd) {
            /* SD path: read CSV file from SD card */
            osMutexAcquire(sd_mutexHandle, osWaitForever);
            fr = f_open(&f, fullpath, FA_READ);
            if (fr != FR_OK) {
                osMutexRelease(sd_mutexHandle);
                CONS_ERR("[MODEM] Cannot re-open CSV for upload: %s (FRESULT=%d)\r\n", fullpath, (int)fr);
                Modem_PowerOff();
                return HAL_ERROR;
            }
            UINT br;
            uint8_t buf[HTTP_CHUNK_SIZE];
            DWORD remaining = datasize;
            while (remaining > 0) {
                WDT_Refresh();
                UINT to_read = remaining > HTTP_CHUNK_SIZE ? HTTP_CHUNK_SIZE : (UINT)remaining;
                if (f_read(&f, buf, to_read, &br) != FR_OK) {
                    f_close(&f);
                    osMutexRelease(sd_mutexHandle);
                    Modem_PowerOff();
                    return HAL_ERROR;
                }
                if (br == 0) break;
                HAL_UART_Transmit(_modem_uart, buf, br, 5000);
                remaining -= br;
            }
            f_close(&f);
            osMutexRelease(sd_mutexHandle);
        }

        HAL_Delay(1000);
        
        // Esperar respuesta (120s)
        // OJO: Con AT+QHTTPCFG="responseheader",1, el modem primero imprime los headers
        // y luego el +QHTTPPOST: 0,200,...
        // Si usamos Modem_WaitFor("+QHTTPPOST:"), podemos perdernos si llega mezclado.
        
        HAL_StatusTypeDef res = Modem_WaitFor("+QHTTPPOST:", 120000);
        
        // Si hay respuesta HTTP pero Modem_WaitFor no la capturo bien (por buffer circular o lo que sea),
        // intentamos leer de todas formas.
        
        CONS_INFO("[MODEM] Reading server response...\r\n");
        // AT+QHTTPREAD= wait_time
        Modem_SendAT("AT+QHTTPREAD=60", "OK", 60000); 

        // Analisis de respuesta mas robusto:
        // Buscamos "+QHTTPPOST:" en todo el buffer acumulado o en la ultima lectura
        char* p = strstr(modem_rx_buffer, "+QHTTPPOST:");
        
        // Si no esta en el buffer actual, quizas ya paso.
        // Pero el log muestra: [MODEM] RAW QHTTPPOST: CONNECT HTTP/1.1 200 OK ...
        // El modem esta devolviendo los HEADERS directamente al UART porque activamos responseheader=1
        // Y el URC +QHTTPPOST: quizas viene DESPUES o ANTES.
        
        // Si vemos codigo 2xx en la respuesta, es EXITO (200 OK, 201 Created, etc.)
        if (strstr(modem_rx_buffer, "HTTP/1.1 2") != NULL || strstr(modem_rx_buffer, "HTTP/1.0 2") != NULL) {
             Modem_CreateDoneMarker(filename);
             CONS_OK("[MODEM] HTTP 2xx detected. Upload successful.\r\n");
             /* Modem_PowerOff() se llama en modem_task después de queue pop */
             return HAL_OK;
        }

        if (res != HAL_OK && p == NULL) {
            CONS_ERR("[MODEM] POST failed (timeout/error).\r\n");
            Modem_PowerOff();
            return res;
        }

        CONS_DBG("[MODEM] RAW QHTTPPOST: %s\r\n", modem_rx_buffer);
        int http_result = -1;
        int http_status = 0;
        
        if (p) {
            p += strlen("+QHTTPPOST:");
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            char* c1 = strchr(p, ',');
            char* c2 = c1 ? strchr(c1 + 1, ',') : NULL;
            if (c1) {
                http_result = atoi(p);
                // Si hay 3 argumentos: err, http_code, content_len
                // Si hay 2 argumentos: err, http_code
                if (c2) {
                     // Formato err,code,len
                    http_status = atoi(c1 + 1);
                } else {
                     // Formato err,code
                    http_status = atoi(c1 + 1);
                }
            }
        }
        
        // Fallback: Si no pudimos parsear +QHTTPPOST pero vimos 200 OK antes, ya retornamos OK.
        // Si llegamos aqui, es que no vimos 200 OK claro.
        
        CONS_DBG("[MODEM] HTTP Result: %d, Status: %d\r\n", http_result, http_status);
        
        // Si el resultado es 0 (Exito modem) y status 200, todo bien.
        if (http_result == 0 && (http_status == 200 || http_status == 0)) {
             Modem_CreateDoneMarker(filename);
             CONS_OK("[MODEM] Upload complete (backend).\r\n");
             /* Modem_PowerOff() se llama en modem_task después de queue pop */
             return HAL_OK;
        }

        if (http_result != 0) {
            Modem_PowerOff();
            return HAL_ERROR;
        }
        if (http_status != 0 && (http_status < 200 || http_status >= 300)) {
            CONS_ERR("[MODEM] HTTP Status out of range: %d\r\n", http_status);
            Modem_PowerOff();
            return HAL_ERROR;
        }
        Modem_CreateDoneMarker(filename);
        CONS_OK("[MODEM] Upload complete (backend, fallthrough).\r\n");
        /* Modem_PowerOff() se llama en modem_task después de queue pop */
        return HAL_OK;
    }

    CONS_WARN("[MODEM] No credentials configured (Drive/Backend).\r\n");
    Modem_PowerOff();
    return HAL_ERROR;
#endif
}

HAL_StatusTypeDef Modem_DownloadConfig(char* out_buffer, uint16_t out_size) {
    if (BACKEND_CONFIG_URL[0] == 0) {
        CONS_WARN("[MODEM] BACKEND_CONFIG_URL not set.\r\n");
        return HAL_ERROR;
    }
    if (out_buffer == NULL || out_size == 0) {
        return HAL_ERROR;
    }
    if (Modem_BringUpNetwork() != HAL_OK) {
        Modem_PowerOff();
        return HAL_ERROR;
    }
    Modem_SendAT("AT+QHTTPCFG=\"contextid\",1", "OK", 1000);
    Modem_SendAT("AT+QHTTPCFG=\"requestheader\",0", "OK", 1000);
    Modem_SendAT("AT+QHTTPCFG=\"responseheader\",0", "OK", 1000);
    Modem_SendAT("AT+QSSLCFG=\"sslversion\",1,4", "OK", 1000);
    Modem_SendAT("AT+QSSLCFG=\"seclevel\",1,0", "OK", 1000);
    Modem_SendAT("AT+QHTTPCFG=\"sslctxid\",1", "OK", 1000);
    char url[256];
    if (BACKEND_API_KEY[0] != 0) {
        snprintf(url, sizeof(url), "%s?name=AWTAS_CONFIG.TXT&key=%s&compact=1", BACKEND_CONFIG_URL, BACKEND_API_KEY);
    } else {
        snprintf(url, sizeof(url), "%s?name=AWTAS_CONFIG.TXT&compact=1", BACKEND_CONFIG_URL);
    }
    CONS_DBG("[MODEM] CFG URL: %s\r\n", url);
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+QHTTPURL=%d,30", (int)strlen(url));
    if (Modem_SendAT(cmd, "CONNECT", 2000) != HAL_OK) {
        Modem_PowerOff();
        return HAL_ERROR;
    }
    HAL_UART_Transmit(_modem_uart, (uint8_t*)url, strlen(url), 2000);
    Modem_SendAT("", "OK", 2000);
    if (Modem_SendAT("AT+QHTTPGET=60", "+QHTTPGET:", 60000) != HAL_OK) {
        Modem_PowerOff();
        return HAL_ERROR;
    }
    WDT_Refresh(); // Refresh tras GET (60s timeout)
    int http_code = 0;
    char* p = strstr(modem_rx_buffer, "+QHTTPGET:");
    if (p != NULL) {
        char* c1 = strchr(p, ',');
        char* c2 = c1 ? strchr(c1 + 1, ',') : NULL;
        if (c1 != NULL && c2 != NULL) {
            http_code = atoi(c1 + 1);
        }
    }
    CONS_INFO("[MODEM] CFG HTTP Status: %d\r\n", http_code);
    if (http_code != 0 && (http_code < 200 || http_code >= 300)) {
        CONS_ERR("[MODEM] CFG bad HTTP status: %d\r\n", http_code);
        Modem_PowerOff();
        return HAL_ERROR;
    }
    const char* read_cmd = "AT+QHTTPREAD=60\r\n";
    HAL_UART_Transmit(_modem_uart, (uint8_t*)read_cmd, strlen(read_cmd), 1000);
    uint32_t start = HAL_GetTick();
    size_t w = 0;
    while ((HAL_GetTick() - start) < 60000) {
        WDT_Refresh(); // Refresh cada ~10ms, evita WDT en descarga de config (60s)
        uint8_t b;
        // CRITICAL FIX: Limpiar ORE (Overrun) si ocurre
        if (__HAL_UART_GET_FLAG(_modem_uart, UART_FLAG_ORE)) {
            __HAL_UART_CLEAR_FLAG(_modem_uart, UART_FLAG_ORE);
        }
        
        // Timeout muy corto para polling rapido
        if (HAL_UART_Receive(_modem_uart, &b, 1, 10) == HAL_OK) {
            if (w < out_size - 1) {
                out_buffer[w++] = (char)b;
            } else {
                // Buffer lleno, seguir leyendo para vaciar FIFO pero no guardar
            }
        }
    }

    if (w >= out_size) w = out_size - 1;
    out_buffer[w] = 0;
    
    // Fix para posible corrupcion de buffer (FILEANUALMATFIL_AUO=MT)
    // Asegurar terminacion de linea y limpieza
    // IMPORTANTE: NO reemplazar \r ni \n, son necesarios para strtok
    for(size_t k=0; k<w; k++) {
        // Permitir tab (9), CR (13), LF (10) y caracteres imprimibles (>=32)
        if(out_buffer[k] < 32 && out_buffer[k] != '\r' && out_buffer[k] != '\n' && out_buffer[k] != '\t') {
            out_buffer[k] = ' '; // Reemplazar caracteres no imprimibles con espacios
        }
    }
    
    CONS_INFO("[MODEM] CFG bytes: %lu\r\n", (unsigned long)w);
    
    // DEBUG: Imprimir buffer crudo para ver que esta llegando
    CONS_DBG("[MODEM] RAW Config Buffer:\r\n%s\r\n", out_buffer);
    
    // Buscar codigo HTTP en "+QHTTPGET: 0,200,..." en el buffer anterior (si existe)
    // Pero out_buffer contiene el contenido, no el header.
    // Usamos el http_code obtenido antes de la lectura de contenido.
    
    // Si http_code es 0, intentamos ver si obtuvimos contenido valido de todas formas.
    // A veces el modem no devuelve el URC correctamente pero si el contenido.
    
    // Solo fallar si codigo es error explicito (no 0). 0 significa 'no disponible' no 'error'.
    if (http_code != 0 && (http_code < 200 || http_code >= 300)) {
        CONS_ERR("[MODEM] CFG HTTP error: %d\r\n", http_code);
        Modem_PowerOff();
        return HAL_ERROR;
    }
    
    // Parsear pares CLAVE=VALOR
    int keys = 0;
    
    char* line = strtok(out_buffer, "\r\n");
    while(line) {
        // Ignorar CONNECT si aparece al principio
        if (strstr(line, "CONNECT") == line) {
            line = strtok(NULL, "\r\n");
            continue;
        }
        
        char* eq = strchr(line, '=');
        if(eq) {
            *eq = 0;
            char* key = line;
            char* val = eq + 1;
            
            while(*key == ' ' || *key == '\t') key++;
            while(*val == ' ' || *val == '\t') val++;
            
            CONS_INFO("[MODEM] CFG %s=%s\r\n", key, val);
            Apply_Remote_Config(key, val);
            keys++;
        }
        line = strtok(NULL, "\r\n");
    }
    
    CONS_OK("[MODEM] CFG %d keys applied.\r\n", keys);
    Modem_PowerOff();
    return (keys > 0) ? HAL_OK : HAL_ERROR;
}
