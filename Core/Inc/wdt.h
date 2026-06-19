/*
 * wdt.h
 *
 * Independent Watchdog (IWDG) for STM32F446RE.
 * Uses direct register access (HAL_IWDG is not enabled in hal_conf).
 * LSI ~32 kHz, prescaler /128 => ~250 Hz, 4095 ticks => ~16.4s timeout.
 *
 * Refresh in every task main loop to prevent reset.
 */

#ifndef INC_WDT_H_
#define INC_WDT_H_

#include <stdint.h>

typedef enum {
    RESET_REASON_NONE = 0,
    RESET_REASON_POWER = 1 << 0,      /* Power-on reset */
    RESET_REASON_PIN = 1 << 1,        /* Pin reset (NRST) */
    RESET_REASON_BOR = 1 << 2,        /* Brown-out reset */
    RESET_REASON_SFTR = 1 << 3,       /* Software reset */
    RESET_REASON_IWDG = 1 << 4,       /* Independent watchdog reset */
    RESET_REASON_WWDG = 1 << 5,       /* Window watchdog reset */
    RESET_REASON_LOWPOWER = 1 << 6,   /* Low-power reset */
} ResetReason;

void WDT_Init(void);
void WDT_Refresh(void);
ResetReason WDT_GetResetReason(void);
void WDT_ClearResetFlags(void);
void WDT_SystemReset(void);

#endif /* INC_WDT_H_ */
