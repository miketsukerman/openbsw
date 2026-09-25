/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

// Arduino Portenta H7 on the Portenta Hat Carrier:
// USART1 on PA9 (TX) / PA10 (RX), AF7 - routed to the 40-pin HAT header
// pins 8 (TX) / 10 (RX). 115200 baud @ 120 MHz APB2 kernel clock.

#include <bsp/UartParams.h>
#include <bsp/uart/UartConfig.h>
#include <etl/error_handler.h>

namespace bsp
{

Uart::UartConfig const Uart::_uartConfigs[] = {
    {
        USART1,               // usart
        GPIOA,                // gpioPort
        9U,                   // txPin (PA9)
        10U,                  // rxPin (PA10)
        7U,                   // af (AF7)
        1042U,                // brr (120000000 / 115200 = 1041.67 -> 1042)
        RCC_AHB4ENR_GPIOAEN,  // rccGpioEnBit
        RCC_APB2ENR_USART1EN, // rccUsartEnBit
        &RCC->AHB4ENR,        // rccGpioEnReg
        &RCC->APB2ENR,        // rccUsartEnReg
    },
};

static Uart instances[] = {
    Uart(Uart::Id::TERMINAL),
};

Uart& Uart::getInstance(Id id)
{
    ETL_ASSERT(
        id < Id::INVALID, ETL_ERROR_GENERIC("UartId::INVALID is not a valid Uart identifier"));
    static_assert(
        NUMBER_OF_UARTS == static_cast<size_t>(etl::size(instances)),
        "Not enough Uart instances defined");
    return instances[static_cast<size_t>(id)];
}

} // namespace bsp
