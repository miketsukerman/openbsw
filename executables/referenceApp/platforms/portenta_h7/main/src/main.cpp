/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "async/Config.h"
#include "clock/clockConfig.h"
#include "lifecycle/StaticBsp.h"
#include "mcu/mcu.h"
#include "systems/CanSystem.h"

#include <etl/alignment.h>
#include <lifecycle/LifecycleManager.h>
#include <safeSupervisor/SafeSupervisor.h>

extern void app_main();

extern "C"
{
void SystemInit()
{
    configurePll();

    // GPIO port clocks used during bring-up: A (console UART), H (FDCAN1
    // PH13/PH14 - FdCanDevice does not enable port clocks itself), K (RGB LED)
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOHEN | RCC_AHB4ENR_GPIOKEN;
    (void)RCC->AHB4ENR; // Read-back for clock propagation

    // Green LED ON: PK6 = LEDG, active low (output push-pull)
    static constexpr uint8_t LED_PIN = 6U;
    GPIOK->MODER &= ~(3U << (LED_PIN * 2U));
    GPIOK->MODER |= (1U << (LED_PIN * 2U));   // GPIO output mode
    GPIOK->BSRR = (1U << (LED_PIN + 16U));    // Drive low -> LED on

    // Early USART1 TX (PA9 AF7, 115200 @ 120 MHz APB2 kernel clock), usable
    // before the BSP UART driver is initialized.
    // BRR = 120 000 000 / 115 200 = 1041.67 -> 1042
    static constexpr uint8_t USART_TX_PIN             = 9U;
    static constexpr uint8_t USART_TX_AF              = 7U; // AF7 = USART1_TX on PA9
    static constexpr uint32_t USART_BRR_115200_120MHZ = 1042U;

    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR; // Read-back for clock propagation
    (void)RCC->APB2ENR;
    GPIOA->MODER &= ~(3U << (USART_TX_PIN * 2U));
    GPIOA->MODER |= (2U << (USART_TX_PIN * 2U)); // Alternate function mode
    GPIOA->AFR[1] &= ~(0xFU << ((USART_TX_PIN - 8U) * 4U));
    GPIOA->AFR[1] |= (static_cast<uint32_t>(USART_TX_AF) << ((USART_TX_PIN - 8U) * 4U));
    USART1->CR1 = 0;
    USART1->CR2 = 0;
    USART1->CR3 = 0;
    USART1->BRR = USART_BRR_115200_120MHZ;
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
    while ((USART1->ISR & USART_ISR_TEACK) == 0) {} // Wait for TX enable ack
}
} // extern "C"

namespace platform
{
StaticBsp staticBsp;

StaticBsp& getStaticBsp() { return staticBsp; }

::etl::typed_storage<::systems::CanSystem> canSystem;

void platformLifecycleAdd(::lifecycle::LifecycleManager& lifecycleManager, uint8_t const level)
{
    if (level == 2U)
    {
        lifecycleManager.addComponent("can", canSystem.create(TASK_CAN), level);
    }
}
} // namespace platform

namespace systems
{
::can::ICanSystem& getCanSystem() { return *::platform::canSystem; }
} // namespace systems

int main()
{
    ::safety::safeSupervisorConstructor.construct();
    ::platform::staticBsp.init();
    app_main();
    return 1;
}
