/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#pragma once

#include <etl/span.h>

#include <cstdint>

namespace bsp
{

/**
 * USB-CDC (virtual COM port) console driver built on TinyUSB.
 *
 * Targets the STM32H747's USB1 OTG_HS controller with its internal
 * full-speed PHY (DP = PB15, DM = PB14, AF12) - the configuration used by
 * the Arduino Portenta H7's USB-C connector.
 *
 * Usage:
 * - call init() once (enables clocks, GPIO, TinyUSB device stack),
 * - call poll() periodically (~1 ms) from task context to service the stack,
 * - OTG_HS_IRQHandler must forward to UsbCdc::interruptHandler().
 *
 * write() is non-blocking: when no host terminal is connected (or its
 * buffer is full) bytes are dropped so logging can never stall the system.
 */
class UsbCdc
{
public:
    /** Enable USB clocks/power/pins and initialize the TinyUSB device stack. */
    static void init();

    /** Service the TinyUSB device stack; call periodically from task context. */
    static void poll();

    /** Forward the OTG_HS interrupt into TinyUSB. */
    static void interruptHandler();

    /**
     * Non-blocking write. Bytes that don't fit (or when no terminal is
     * connected) are dropped.
     * \return number of bytes accepted
     */
    static uint32_t write(::etl::span<uint8_t const> data);

    /**
     * Non-blocking read.
     * \return number of bytes read (0 if none available)
     */
    static uint32_t read(::etl::span<uint8_t> data);
};

} // namespace bsp
