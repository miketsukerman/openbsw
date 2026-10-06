/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "systems/UsbSystem.h"

#include "bsp/usb/UsbCdc.h"
#include "mcu/mcu.h"

namespace
{
// NVIC priority for the OTG_HS (USB1) interrupt. Must stay numerically >=
// configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (0x06 in
// FreeRtosPlatformConfig.h) so the ISR may safely use FromISR APIs.
constexpr uint32_t USB_IRQ_PRIORITY = 6U;

// TinyUSB device task poll period. 1 ms keeps the console responsive while
// adding negligible load (tud_task returns immediately when idle).
constexpr uint32_t USB_POLL_PERIOD_MS = 1U;
} // namespace

namespace systems
{

UsbSystem::UsbSystem(::async::ContextType const context)
: ::lifecycle::SingleContextLifecycleComponent(context)
, _context(context)
, _pollRunnable(*this)
, _pollTimeout()
{}

void UsbSystem::init() { transitionDone(); }

void UsbSystem::run()
{
    // Brings up HSI48/CRS, the USB pins/clock and the TinyUSB device stack.
    // Fail-safe: if anything fails, UsbCdc stays uninitialized and poll(),
    // write() and read() become no-ops - the system keeps running.
    ::bsp::UsbCdc::init();

    NVIC_SetPriority(OTG_HS_IRQn, USB_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(OTG_HS_IRQn);
    NVIC_EnableIRQ(OTG_HS_IRQn);

    ::async::scheduleAtFixedRate(
        _context, _pollRunnable, _pollTimeout, USB_POLL_PERIOD_MS, ::async::TimeUnit::MILLISECONDS);

    transitionDone();
}

void UsbSystem::shutdown()
{
    _pollTimeout.cancel();
    NVIC_DisableIRQ(OTG_HS_IRQn);
    transitionDone();
}

void UsbSystem::poll() { ::bsp::UsbCdc::poll(); }

} // namespace systems
