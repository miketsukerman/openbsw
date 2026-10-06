/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "systems/PortentaEthernetSystem.h"

#include "ethConfig.h"
#include "ethernet/EthernetLogger.h"
#include "mcu/mcu.h"

using namespace ::util::logger;

namespace
{
constexpr uint32_t LINK_POLL_PERIOD_MS = 100U;

// NVIC priority for the ETH interrupt. Must stay numerically >=
// configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (0x06) so the ISR may safely
// use FreeRTOS FromISR APIs.
constexpr uint32_t ETH_IRQ_PRIORITY = 6U;

// LAN8742AI PHY address on the Portenta H7 module
constexpr uint8_t PHY_ADDRESS = 0U;

constexpr size_t NUM_RX_DESCRIPTORS = 8U;
constexpr size_t NUM_TX_DESCRIPTORS = 8U;

// DMA descriptors and frame buffers in D2 SRAM3 (0x30040000, 32 KiB). The
// section is NOLOAD; the driver initializes every descriptor before starting
// the DMA and the buffers carry no state across frames. SRAM3 is configured
// non-cacheable via the MPU (see init()) so the Ethernet DMA and the
// D-cache-enabled CM7 core always see the same data.
// clang-format off
__attribute__((section(".eth_sram"), aligned(32)))
::ethernet::EthDmaDescriptor rxDescriptors[NUM_RX_DESCRIPTORS];
__attribute__((section(".eth_sram"), aligned(32)))
::ethernet::EthDmaDescriptor txDescriptors[NUM_TX_DESCRIPTORS];
__attribute__((section(".eth_sram"), aligned(32)))
uint8_t rxBuffers[NUM_RX_DESCRIPTORS * ::ethernet::EthDriver::BUFFER_SIZE];
__attribute__((section(".eth_sram"), aligned(32)))
uint8_t txBuffers[NUM_TX_DESCRIPTORS * ::ethernet::EthDriver::BUFFER_SIZE];
// clang-format on

constexpr uint32_t ETH_SRAM_BASE      = 0x30040000UL;
constexpr uint8_t ETH_SRAM_SIZE_LOG2  = 15U; // 32 KiB
constexpr uint8_t ETH_SRAM_MPU_REGION = 0U;

// CPU-only bookkeeping (normal, cached RAM)
::lwiputils::RxCustomPbuf rxPbufs[NUM_RX_DESCRIPTORS];
pbuf* rxPbufAtIndex[NUM_RX_DESCRIPTORS];

struct RmiiPin
{
    GPIO_TypeDef* port;
    uint8_t pin;
};

// RMII pinout of the Portenta H7 (STM32H747 <-> LAN8742AI), all AF11.
constexpr uint32_t RMII_AF        = 11U;
constexpr RmiiPin RMII_PINS[]     = {
    {GPIOA, 1U},  // ETH_RMII_REF_CLK (50 MHz from the PHY)
    {GPIOA, 2U},  // ETH_MDIO
    {GPIOA, 7U},  // ETH_RMII_CRS_DV
    {GPIOC, 1U},  // ETH_MDC
    {GPIOC, 4U},  // ETH_RMII_RXD0
    {GPIOC, 5U},  // ETH_RMII_RXD1
    {GPIOG, 11U}, // ETH_RMII_TX_EN
    {GPIOG, 12U}, // ETH_RMII_TXD1
    {GPIOG, 13U}, // ETH_RMII_TXD0
};

void configureRmiiPin(GPIO_TypeDef* const port, uint8_t const pin)
{
    port->MODER = (port->MODER & ~(3U << (pin * 2U))) | (2U << (pin * 2U)); // AF mode
    port->OSPEEDR |= (3U << (pin * 2U));                                    // Very high speed
    port->PUPDR &= ~(3U << (pin * 2U));                                     // No pull
    uint8_t const idx   = (pin < 8U) ? 0U : 1U;
    uint8_t const shift = static_cast<uint8_t>((pin % 8U) * 4U);
    port->AFR[idx]      = (port->AFR[idx] & ~(0xFUL << shift)) | (RMII_AF << shift);
}

void busyWaitApproxMs(uint32_t const milliseconds)
{
    // ~480 cycles/us at 480 MHz; conservative busy wait used only during boot
    for (uint32_t volatile i = 0U; i < (milliseconds * 500000U); ++i) {}
}

::ethernet::EthDriver* pEthDriver = nullptr;
} // namespace

extern "C"
{
void ethIsr()
{
    if (pEthDriver != nullptr)
    {
        pEthDriver->interrupt();
    }
}
}

namespace systems
{

PortentaEthernetSystem::PortentaEthernetSystem(::async::ContextType const context)
: ::lifecycle::SingleContextLifecycleComponent(context)
, _context(context)
, _pollRunnable(*this)
, _pollTimeout()
, _driverConfig{
    PHY_ADDRESS,
    rxDescriptors,
    txDescriptors,
    rxBuffers,
    txBuffers,
    rxPbufs,
    rxPbufAtIndex,
}
, _driver(::ethX::MAC_ADDRESS, _driverConfig)
{}

void PortentaEthernetSystem::init()
{
    // GPIO ports: A/C/G (RMII pins), H (PH1 oscillator enable, shared with
    // the USB ULPI PHY), J (PJ15 Ethernet PHY power/reset)
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIOGEN
                    | RCC_AHB4ENR_GPIOHEN | RCC_AHB4ENR_GPIOJEN;
    (void)RCC->AHB4ENR; // Read-back for clock propagation

    // D2 SRAM3 clock (descriptor/buffer memory)
    RCC->AHB2ENR |= RCC_AHB2ENR_D2SRAM3EN;
    (void)RCC->AHB2ENR;

    // PH1 enables the on-board oscillator clocking the Ethernet PHY (and the
    // USB ULPI PHY). Idempotent if the USB console already switched it on.
    GPIOH->MODER = (GPIOH->MODER & ~(3U << (1U * 2U))) | (1U << (1U * 2U));
    GPIOH->BSRR  = (1U << 1U);

    // PJ15 powers/releases the LAN8742AI. Give the oscillator and the PHY
    // time to start before talking to it (LAN8742 needs ~0.5 ms after reset).
    GPIOJ->MODER = (GPIOJ->MODER & ~(3U << (15U * 2U))) | (1U << (15U * 2U));
    GPIOJ->BSRR  = (1U << 15U);
    busyWaitApproxMs(10U);

    // RMII pins
    for (RmiiPin const& rmiiPin : RMII_PINS)
    {
        configureRmiiPin(rmiiPin.port, rmiiPin.pin);
    }

    // Select RMII mode in SYSCFG before enabling the MAC clocks
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;
    SYSCFG->PMCR = (SYSCFG->PMCR & ~SYSCFG_PMCR_EPIS_SEL) | SYSCFG_PMCR_EPIS_SEL_2;

    // Ethernet MAC + TX/RX clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_ETH1MACEN | RCC_AHB1ENR_ETH1TXEN | RCC_AHB1ENR_ETH1RXEN;
    (void)RCC->AHB1ENR;

    // Descriptors/buffers must not be cached: dedicate an MPU region to SRAM3
    ::ethernet::EthDriver::configureMpuNonCacheable(
        ETH_SRAM_BASE, ETH_SRAM_SIZE_LOG2, ETH_SRAM_MPU_REGION);

    _initialized = _driver.init();
    if (!_initialized)
    {
        Logger::error(ETHERNET, "Portenta Ethernet bring-up failed");
    }

    transitionDone();
}

void PortentaEthernetSystem::run()
{
    if (_initialized)
    {
        _driver.start();
        pEthDriver = &_driver;

        NVIC_SetPriority(ETH_IRQn, ETH_IRQ_PRIORITY);
        NVIC_ClearPendingIRQ(ETH_IRQn);
        NVIC_EnableIRQ(ETH_IRQn);

        ::async::scheduleAtFixedRate(
            _context,
            _pollRunnable,
            _pollTimeout,
            LINK_POLL_PERIOD_MS,
            ::async::TimeUnit::MILLISECONDS);
    }
    transitionDone();
}

void PortentaEthernetSystem::shutdown()
{
    _pollTimeout.cancel();
    if (_initialized)
    {
        NVIC_DisableIRQ(ETH_IRQn);
        _driver.stop();
        pEthDriver = nullptr;
    }
    transitionDone();
}

void PortentaEthernetSystem::pollLink()
{
    bool const isLinkUp = _driver.pollLink();
    if (isLinkUp != _linkUp)
    {
        _linkUp = isLinkUp;
        Logger::info(ETHERNET, "LAN8742 link status changed to %s", (isLinkUp ? "up" : "down"));
    }
}

bool PortentaEthernetSystem::getLinkStatus(size_t const port)
{
    // This platform only supports one port.
    if (port == 0U)
    {
        return _linkUp;
    }
    return false;
}

} // namespace systems
