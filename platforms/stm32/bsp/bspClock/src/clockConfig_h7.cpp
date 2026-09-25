/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

// Clock and power configuration for STM32H747 (Cortex-M7 core) on the
// Arduino Portenta H7.
//
// Power scheme: the Portenta H7 powers VCORE from the internal LDO
// (PWR_LDO_SUPPLY); the H747 internal SMPS is not used for VCORE. VOS0
// (boost) is required for 480 MHz and is entered via the SYSCFG overdrive
// enable, per RM0399.
//
// Clock tree: HSE 25 MHz (external oscillator) -> PLL1
//   DIVM1 = 5   -> 5 MHz PLL reference (range 4-8 MHz)
//   N1    = 192 -> VCO 960 MHz (wide range)
//   P1    = 2   -> sys_ck 480 MHz
//   Q1    = 8   -> pll1_q 120 MHz
//   D1CPRE /1, HPRE /2 -> AXI/AHB 240 MHz; all APB prescalers /2 -> 120 MHz.
//
// The Cortex-M4 core is intentionally left untouched: on the Portenta H7 the
// factory configuration does not run application code on the CM4, and option
// bytes (BCM4) are never modified here to avoid bricking the board.

#include <mcu/mcu.h>

uint32_t SystemCoreClock = 64000000U; // Default HSI, updated by configurePll()

// Clock stabilization timeout (~100 ms at 64 MHz HSI = 6.4M cycles).
// On timeout the system stays on HSI 64 MHz and SystemCoreClock is NOT
// updated, indicating to downstream code that PLL init failed.
static constexpr uint32_t CLK_TIMEOUT = 6400000U;

namespace
{
bool waitSet(uint32_t volatile const& reg, uint32_t mask)
{
    uint32_t t = CLK_TIMEOUT;
    while ((reg & mask) != mask)
    {
        --t;
        if (t == 0U)
        {
            return false;
        }
    }
    return true;
}

bool enableHse()
{
    // The Portenta H7 feeds OSC_IN from an external 25 MHz oscillator module,
    // so bypass mode is tried first; crystal mode is the fallback.
    RCC->CR |= RCC_CR_HSEBYP;
    RCC->CR |= RCC_CR_HSEON;
    if (waitSet(RCC->CR, RCC_CR_HSERDY))
    {
        return true;
    }
    RCC->CR &= ~RCC_CR_HSEON;
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CR |= RCC_CR_HSEON;
    return waitSet(RCC->CR, RCC_CR_HSERDY);
}
} // namespace

extern "C" void configurePll()
{
    // --- Supply configuration: LDO supplies VCORE (Portenta H7) -------------
    // Must be programmed once out of reset before any VOS change; SMPSEN and
    // BYPASS are cleared, LDOEN set (equivalent of HAL PWR_LDO_SUPPLY).
    PWR->CR3 = (PWR->CR3 & ~(PWR_CR3_SMPSEN | PWR_CR3_BYPASS)) | PWR_CR3_LDOEN;
    if (!waitSet(PWR->CSR1, PWR_CSR1_ACTVOSRDY))
    {
        return;
    }

    // --- Voltage scaling: VOS1, then VOS0 via SYSCFG overdrive --------------
    PWR->D3CR = (PWR->D3CR & ~PWR_D3CR_VOS) | PWR_D3CR_VOS; // VOS = 0b11 (Scale 1)
    if (!waitSet(PWR->D3CR, PWR_D3CR_VOSRDY))
    {
        return;
    }

    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    uint32_t volatile dummy = RCC->APB4ENR; // Read-back for clock enable delay
    (void)dummy;

    SYSCFG->PWRCR |= SYSCFG_PWRCR_ODEN; // Enter VOS0 (480 MHz boost)
    if (!waitSet(PWR->D3CR, PWR_D3CR_VOSRDY))
    {
        return;
    }

    // --- HSE 25 MHz ---------------------------------------------------------
    if (!enableHse())
    {
        return;
    }

    // --- PLL1: 25 MHz / M=5 * N=192 / P=2 = 480 MHz --------------------------
    RCC->PLLCKSELR = RCC_PLLCKSELR_PLLSRC_HSE | (5U << RCC_PLLCKSELR_DIVM1_Pos);

    // Reference 4-8 MHz (RGE=0b10), wide VCO, enable P and Q outputs
    RCC->PLLCFGR = RCC_PLLCFGR_PLL1RGE_2 | RCC_PLLCFGR_DIVP1EN | RCC_PLLCFGR_DIVQ1EN;

    RCC->PLL1DIVR = ((192U - 1U) << RCC_PLL1DIVR_N1_Pos) | ((2U - 1U) << RCC_PLL1DIVR_P1_Pos)
                    | ((8U - 1U) << RCC_PLL1DIVR_Q1_Pos) | ((2U - 1U) << RCC_PLL1DIVR_R1_Pos);

    RCC->CR |= RCC_CR_PLL1ON;
    if (!waitSet(RCC->CR, RCC_CR_PLL1RDY))
    {
        return;
    }

    // --- Bus prescalers (program before the sysclk switch) ------------------
    // D1: CPU /1, AXI/AHB /2 (240 MHz), APB3 /2 (120 MHz)
    RCC->D1CFGR = RCC_D1CFGR_HPRE_DIV2 | RCC_D1CFGR_D1PPRE_DIV2;
    // D2: APB1 /2, APB2 /2 (120 MHz)
    RCC->D2CFGR = RCC_D2CFGR_D2PPRE1_DIV2 | RCC_D2CFGR_D2PPRE2_DIV2;
    // D3: APB4 /2 (120 MHz)
    RCC->D3CFGR = RCC_D3CFGR_D3PPRE_DIV2;

    // --- Flash wait states: 4 WS + WRHIGHFREQ=0b10 for 240 MHz AXI @ VOS0 ---
    {
        uint32_t acr = FLASH->ACR;
        acr &= ~(FLASH_ACR_LATENCY_Msk | FLASH_ACR_WRHIGHFREQ_Msk);
        acr |= FLASH_ACR_LATENCY_4WS | FLASH_ACR_WRHIGHFREQ_1;
        FLASH->ACR = acr;
    }
    if ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_4WS)
    {
        return;
    }

    // --- Switch sysclk to PLL1 ----------------------------------------------
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL1;
    if (!waitSet(RCC->CFGR, RCC_CFGR_SWS_PLL1))
    {
        return;
    }

    // --- Caches --------------------------------------------------------------
    // SRAMCAN (FDCAN message RAM) and all peripheral registers live in the
    // default Device memory region, which the Cortex-M7 never caches, so no
    // MPU configuration is required for coherence with the CAN driver.
    SCB_EnableICache();
    SCB_EnableDCache();

    SystemCoreClock = 480000000U;
}
