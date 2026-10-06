/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "bsp/usb/UsbCdc.h"

#include "clock/clockConfig.h"

#include <mcu/mcu.h>
#include <tusb.h>

// Not pulled from device/dcd.h: that header triggers -Wmissing-field-initializers
// warnings in C++ translation units. The symbol is a stable TinyUSB API.
extern "C" void dcd_int_handler(uint8_t rhport);

namespace
{
// On the STM32H747 both OTG controllers exist, so TinyUSB's STM32 port maps
// rhport 0 to USB2 OTG_FS and rhport 1 to USB1 OTG_HS. The Portenta H7's
// USB-C connector is wired to USB1 OTG_HS (internal FS PHY).
constexpr uint8_t USB_RHPORT = 1U;

bool initialized = false;
} // namespace

namespace bsp
{

void UsbCdc::init()
{
    // 48 MHz USB kernel clock (HSI48 + CRS). If it fails, skip USB entirely -
    // the system must keep running without a console device.
    if (configureUsbClock() == 0)
    {
        return;
    }

    // Enable the USB 3.3 V voltage level detector (required before enabling
    // the USB peripheral on the H7).
    PWR->CR3 |= PWR_CR3_USB33DEN;
    {
        uint32_t timeout = 100000U;
        while (((PWR->CR3 & PWR_CR3_USB33RDY) == 0U) && (timeout > 0U))
        {
            --timeout;
        }
        if (timeout == 0U)
        {
            return;
        }
    }

    // USB D- (PB14) / D+ (PB15), AF12 (OTG_HS in FS mode, internal PHY)
    constexpr uint8_t DM_PIN = 14U;
    constexpr uint8_t DP_PIN = 15U;
    constexpr uint8_t USB_AF = 12U;

    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    (void)RCC->AHB4ENR; // Read-back for clock propagation

    GPIOB->MODER &= ~((3U << (DM_PIN * 2U)) | (3U << (DP_PIN * 2U)));
    GPIOB->MODER |= (2U << (DM_PIN * 2U)) | (2U << (DP_PIN * 2U));   // AF mode
    GPIOB->OSPEEDR |= (3U << (DM_PIN * 2U)) | (3U << (DP_PIN * 2U)); // Very high speed
    GPIOB->AFR[1] &= ~((0xFUL << ((DM_PIN - 8U) * 4U)) | (0xFUL << ((DP_PIN - 8U) * 4U)));
    GPIOB->AFR[1] |= (static_cast<uint32_t>(USB_AF) << ((DM_PIN - 8U) * 4U))
                     | (static_cast<uint32_t>(USB_AF) << ((DP_PIN - 8U) * 4U));

    // USB1 OTG_HS peripheral clock
    RCC->AHB1ENR |= RCC_AHB1ENR_USB1OTGHSEN;
    (void)RCC->AHB1ENR; // Read-back for clock propagation

    // TinyUSB: device stack on USB1 OTG_HS. dcd_init() selects the internal
    // FS PHY (CFG_TUD_MAX_SPEED = full speed), forces device mode, and
    // overrides B-session valid (no VBUS sensing wired on the Portenta).
    initialized = tud_init(USB_RHPORT);
}

void UsbCdc::poll()
{
    if (initialized)
    {
        tud_task();
    }
}

void UsbCdc::interruptHandler()
{
    if (initialized)
    {
        dcd_int_handler(USB_RHPORT);
    }
}

uint32_t UsbCdc::write(::etl::span<uint8_t const> const data)
{
    if (!initialized || !tud_cdc_connected())
    {
        // Drop bytes when no terminal is attached - logging must never block.
        return static_cast<uint32_t>(data.size());
    }

    uint32_t const written = tud_cdc_write(data.data(), static_cast<uint32_t>(data.size()));
    tud_cdc_write_flush();
    return written;
}

uint32_t UsbCdc::read(::etl::span<uint8_t> const data)
{
    if (!initialized || !tud_cdc_available())
    {
        return 0U;
    }
    return tud_cdc_read(data.data(), static_cast<uint32_t>(data.size()));
}

} // namespace bsp
