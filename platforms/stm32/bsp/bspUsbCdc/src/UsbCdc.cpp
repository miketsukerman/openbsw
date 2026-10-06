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
// USB-C data lines are wired to an external USB3320 ULPI high-speed PHY
// connected to USB1 OTG_HS; the internal full-speed PHY (PB14/PB15) is not
// routed to the connector.
constexpr uint8_t USB_RHPORT = 1U;

// All OTG_HS ULPI signals use alternate function 10.
constexpr uint32_t ULPI_AF = 10U;

bool initialized = false;

struct UlpiPin
{
    GPIO_TypeDef* port;
    uint8_t pin;
};

// ULPI bus wiring on the Portenta H7 (matches the Arduino core and the
// Zephyr board definition).
constexpr UlpiPin ULPI_PINS[] = {
    {GPIOA, 3U},  // ULPI_D0
    {GPIOA, 5U},  // ULPI_CK (60 MHz from the USB3320)
    {GPIOB, 0U},  // ULPI_D1
    {GPIOB, 1U},  // ULPI_D2
    {GPIOB, 5U},  // ULPI_D7
    {GPIOB, 10U}, // ULPI_D3
    {GPIOB, 11U}, // ULPI_D4
    {GPIOB, 12U}, // ULPI_D5
    {GPIOB, 13U}, // ULPI_D6
    {GPIOC, 0U},  // ULPI_STP
    {GPIOH, 4U},  // ULPI_NXT
    {GPIOI, 11U}, // ULPI_DIR
};

void configureUlpiPin(GPIO_TypeDef* const port, uint8_t const pin)
{
    port->MODER = (port->MODER & ~(3U << (pin * 2U))) | (2U << (pin * 2U)); // AF mode
    port->OSPEEDR |= (3U << (pin * 2U));                                    // Very high speed
    port->PUPDR &= ~(3U << (pin * 2U));                                     // No pull
    uint8_t const idx   = (pin < 8U) ? 0U : 1U;
    uint8_t const shift = static_cast<uint8_t>((pin % 8U) * 4U);
    port->AFR[idx]      = (port->AFR[idx] & ~(0xFUL << shift)) | (ULPI_AF << shift);
}
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

    // GPIO clocks for all ports carrying ULPI signals plus PH1 (oscillator
    // enable).
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOCEN
                    | RCC_AHB4ENR_GPIOHEN | RCC_AHB4ENR_GPIOIEN;
    (void)RCC->AHB4ENR; // Read-back for clock propagation

    // PH1 enables the on-board oscillator that clocks the USB3320 ULPI PHY
    // (and the Ethernet PHY). Drive it high and give the oscillator and PHY
    // time to start before initializing the OTG core: the core soft reset
    // needs the 60 MHz ULPI clock to complete.
    GPIOH->MODER = (GPIOH->MODER & ~(3U << (1U * 2U))) | (1U << (1U * 2U)); // Output
    GPIOH->BSRR  = (1U << 1U);
    {
        // ~2 ms at 480 MHz; generous oscillator/PHY start-up margin.
        for (uint32_t volatile i = 0U; i < 1000000U; ++i) {}
    }

    // ULPI bus pins, AF10, very high speed.
    for (UlpiPin const& ulpiPin : ULPI_PINS)
    {
        configureUlpiPin(ulpiPin.port, ulpiPin.pin);
    }

    // USB1 OTG_HS peripheral clock + ULPI interface clock.
    RCC->AHB1ENR |= RCC_AHB1ENR_USB1OTGHSEN | RCC_AHB1ENR_USB1OTGHSULPIEN;
    (void)RCC->AHB1ENR; // Read-back for clock propagation

    // TinyUSB: device stack on USB1 OTG_HS. dcd_init() selects the ULPI
    // interface (CFG_TUD_MAX_SPEED = high speed + the core reports a ULPI
    // HS PHY), forces device mode, and overrides B-session valid (no VBUS
    // sensing wired on the Portenta).
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
