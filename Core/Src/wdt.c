/*
 * wdt.c
 *
 * IWDG using direct register access.
 * STM32F446RE: IWDG base = APB1PERIPH_BASE + 0x3000
 *
 * IWDG registers:
 *   KR  (0x00) - Key register (write-only)
 *   PR  (0x04) - Prescaler
 *   RLR (0x08) - Reload
 *   SR  (0x0C) - Status
 *
 * KR keys:
 *   0x5555 - Enable write access to PR/RLR
 *   0xAAAA - Reload counter (pet the watchdog)
 *   0xCCCC - Start IWDG
 *
 * PR values (bits 2:0):
 *   0 = /4,  1 = /8,   2 = /16,  3 = /32
 *   4 = /64, 5 = /128, 6 = /256
 *
 * SR bits:
 *   0 = PVU (Prescaler Update in progress)
 *   1 = RVU (Reload Update in progress)
 */

#include "wdt.h"
#include "stm32f4xx_hal.h"   /* for IWDG base addr, register struct */
#include "stm32f4xx.h"       /* for NVIC_SystemReset */

#define IWDG_KR_WRITE_ACCESS_ENABLE  0x5555UL
#define IWDG_KR_RELOAD               0xAAAAUL
#define IWDG_KR_START                0xCCCCUL

/* Get the reset reason from RCC flags */
ResetReason WDT_GetResetReason(void)
{
    ResetReason reason = RESET_REASON_NONE;
    uint32_t rcc_csr = RCC->CSR;

    if (rcc_csr & RCC_CSR_PORRSTF) reason |= RESET_REASON_POWER;
    if (rcc_csr & RCC_CSR_PINRSTF) reason |= RESET_REASON_PIN;
    if (rcc_csr & RCC_CSR_BORRSTF) reason |= RESET_REASON_BOR;
    if (rcc_csr & RCC_CSR_SFTRSTF) reason |= RESET_REASON_SFTR;
    if (rcc_csr & RCC_CSR_IWDGRSTF) reason |= RESET_REASON_IWDG;
    if (rcc_csr & RCC_CSR_WWDGRSTF) reason |= RESET_REASON_WWDG;
    if (rcc_csr & RCC_CSR_LPWRRSTF) reason |= RESET_REASON_LOWPOWER;

    return reason;
}

/* Clear reset flags after reading */
void WDT_ClearResetFlags(void)
{
    RCC->CSR |= RCC_CSR_RMVF;
}

/* System reset using NVIC */
void WDT_SystemReset(void)
{
    NVIC_SystemReset();
}

void WDT_Init(void)
{
    /* Ensure LSI oscillator is enabled and stable */
    RCC->CSR |= RCC_CSR_LSION;
    while (!(RCC->CSR & RCC_CSR_LSIRDY)) { }

    /* Enable write access to PR and RLR */
    IWDG->KR = IWDG_KR_WRITE_ACCESS_ENABLE;

    /* Wait for PVU to clear (may be set from previous config) */
    while (IWDG->SR & IWDG_SR_PVU) { }

    /* Prescaler = /256 => bit 2:0 = 6 (~32s at 32 kHz) */
    IWDG->PR = 6;

    /* Wait for RVU to clear */
    while (IWDG->SR & IWDG_SR_RVU) { }

    /* Reload = 4095 (max, gives ~32.8s with /256 at 32 kHz) */
    IWDG->RLR = 4095;

    /* Start IWDG */
    IWDG->KR = IWDG_KR_START;

    /* First reload to load RLR into counter */
    IWDG->KR = IWDG_KR_RELOAD;
}

void WDT_Refresh(void)
{
    IWDG->KR = IWDG_KR_RELOAD;
}
