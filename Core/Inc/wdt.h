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

void WDT_Init(void);
void WDT_Refresh(void);

#endif /* INC_WDT_H_ */
