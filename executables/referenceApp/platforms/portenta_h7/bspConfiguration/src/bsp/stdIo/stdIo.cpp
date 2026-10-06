/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <cstdint>

#if defined(CONSOLE_OVER_USB_CDC)

#include <bsp/usb/UsbCdc.h>

// Console on the USB-C connector (CDC-ACM virtual COM port, /dev/ttyACM0).
// Bytes written before a host terminal is attached are dropped by design -
// logging must never block. Early-boot output (before USB enumeration) is
// only visible on the UART (USART1) via the SystemInit banner.
extern "C" void putByteToStdout(uint8_t const byte)
{
    (void)bsp::UsbCdc::write(etl::span<uint8_t const>(&byte, 1U));
}

extern "C" int32_t getByteFromStdin()
{
    uint8_t dataByte = 0;
    if (bsp::UsbCdc::read(etl::span<uint8_t>(&dataByte, 1U)) == 0U)
    {
        return -1;
    }
    return dataByte;
}

#else // UART console (USART1, Hat Carrier 40-pin header pins 8/10)

#include "bsp/uart/UartConfig.h"

extern "C" void putByteToStdout(uint8_t const byte)
{
    static bsp::Uart& uart = bsp::Uart::getInstance(bsp::Uart::Id::TERMINAL);
    uart.write(etl::span<uint8_t const>(&byte, 1U));
}

extern "C" int32_t getByteFromStdin()
{
    static bsp::Uart& uart = bsp::Uart::getInstance(bsp::Uart::Id::TERMINAL);
    uint8_t dataByte       = 0;
    if (uart.read(etl::span<uint8_t>(&dataByte, 1U)) == 0U)
    {
        return -1;
    }
    return dataByte;
}

#endif
