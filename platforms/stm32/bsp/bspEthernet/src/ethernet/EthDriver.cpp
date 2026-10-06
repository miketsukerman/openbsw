/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "ethernet/EthDriver.h"

#include "ethernet/EthernetLogger.h"
#include "interrupts/SuspendResumeAllInterruptsScopedLock.h"
#include "mcu/mcu.h"

#include <etl/error_handler.h>

#include <lwip/pbuf.h>

using namespace ::util::logger;

namespace
{
// RX descriptor read format (armed for DMA)
constexpr uint32_t RX_DES3_OWN   = (1UL << 31);
constexpr uint32_t RX_DES3_IOC   = (1UL << 30);
constexpr uint32_t RX_DES3_BUF1V = (1UL << 24);
// RX descriptor write-back format
constexpr uint32_t RX_DES3_FD     = (1UL << 29);
constexpr uint32_t RX_DES3_LD     = (1UL << 28);
constexpr uint32_t RX_DES3_ES     = (1UL << 15);
constexpr uint32_t RX_DES3_PL_MSK = 0x7FFFUL;

// TX descriptor read format
constexpr uint32_t TX_DES3_OWN    = (1UL << 31);
constexpr uint32_t TX_DES3_FD     = (1UL << 29);
constexpr uint32_t TX_DES3_LD     = (1UL << 28);
constexpr uint32_t TX_DES2_B1L_MSK = 0x3FFFUL;

// IEEE 802.3 PHY registers
constexpr uint8_t PHY_REG_BMCR = 0U;
constexpr uint8_t PHY_REG_BMSR = 1U;
// LAN8742 PHY Special Control/Status register: speed indication in bits [4:2]
constexpr uint8_t PHY_REG_PHYSCSR = 31U;

constexpr uint16_t PHY_BMCR_RESET     = 0x8000U;
constexpr uint16_t PHY_BMCR_ANENABLE  = 0x1000U;
constexpr uint16_t PHY_BMCR_ANRESTART = 0x0200U;
constexpr uint16_t PHY_BMSR_LINK_UP   = 0x0004U;

constexpr uint32_t BUSY_WAIT_LIMIT = 100000U;

void acknowledgeDmaInterrupts()
{
    // DMACSR is write-1-to-clear
    uint32_t const status = ETH->DMACSR;
    ETH->DMACSR           = status;
}

} // namespace

extern "C"
{
/**
 * lwIP custom-pbuf free hook: re-arms the RX descriptor owning this buffer.
 */
static void ethFreeCustomPbuf(pbuf* const p)
{
    // RxCustomPbuf embeds pbuf_custom as its first member; address identity is
    // guaranteed.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* const customPbuf = reinterpret_cast<::lwiputils::RxCustomPbuf*>(p);
    auto* const driver     = static_cast<::ethernet::EthDriver*>(customPbuf->driver);
    driver->reclaimRxBuffer(customPbuf);
}
}

namespace ethernet
{

EthDriver::EthDriver(::etl::array<uint8_t, 6> const macAddr, Configuration const& configuration)
: _macAddr(macAddr), _config(configuration)
{}

void EthDriver::configureMpuNonCacheable(
    uint32_t const baseAddress, uint8_t const sizeLog2, uint8_t const region)
{
    __DMB();
    MPU->CTRL = 0U; // disable MPU while reprogramming
    MPU->RNR  = region;
    MPU->RBAR = baseAddress & MPU_RBAR_ADDR_Msk;
    // Normal memory, non-cacheable (TEX=0b001, C=0, B=0, S=0), full access,
    // execute never.
    MPU->RASR = MPU_RASR_ENABLE_Msk | (static_cast<uint32_t>(sizeLog2 - 1U) << MPU_RASR_SIZE_Pos)
                | (0x3UL << MPU_RASR_AP_Pos) | (0x1UL << MPU_RASR_TEX_Pos) | MPU_RASR_XN_Msk;
    // Keep the default memory map for all other addresses.
    MPU->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
    // Drop any stale cache lines covering the now non-cacheable region.
    SCB_CleanInvalidateDCache();
}

bool EthDriver::init()
{
    // Software reset of MAC/MTL/DMA. Completion requires that the RMII
    // REF_CLK from the PHY is running, so time out instead of hanging to stay
    // fail-safe when no clock is present.
    ETH->DMAMR |= ETH_DMAMR_SWR;
    uint32_t retries = BUSY_WAIT_LIMIT;
    while (((ETH->DMAMR & ETH_DMAMR_SWR) != 0U) && (retries > 0U))
    {
        --retries;
    }
    if (retries == 0U)
    {
        Logger::error(ETHERNET, "ETH DMA reset timed out (no RMII REF_CLK?)");
        return false;
    }

    // MDIO clock range for HCLK = 150..250 MHz
    ETH->MACMDIOAR = ETH_MACMDIOAR_CR_DIV102;

    // Reset the PHY and restart auto-negotiation.
    if (!phyWrite(PHY_REG_BMCR, PHY_BMCR_RESET))
    {
        Logger::error(ETHERNET, "ETH PHY reset failed");
        return false;
    }
    uint16_t bmcr = PHY_BMCR_RESET;
    retries       = BUSY_WAIT_LIMIT;
    while (((bmcr & PHY_BMCR_RESET) != 0U) && (retries > 0U))
    {
        (void)phyRead(PHY_REG_BMCR, bmcr);
        --retries;
    }
    (void)phyWrite(PHY_REG_BMCR, PHY_BMCR_ANENABLE | PHY_BMCR_ANRESTART);

    // MAC address (perfect filter entry 0)
    ETH->MACA0HR = (static_cast<uint32_t>(_macAddr[5]) << 8) | _macAddr[4];
    ETH->MACA0LR = (static_cast<uint32_t>(_macAddr[3]) << 24)
                   | (static_cast<uint32_t>(_macAddr[2]) << 16)
                   | (static_cast<uint32_t>(_macAddr[1]) << 8) | _macAddr[0];

    // Strip the FCS of received frames; default to 100M full duplex until the
    // first link-up poll reports the negotiated mode.
    ETH->MACCR = ETH_MACCR_ACS | ETH_MACCR_CST | ETH_MACCR_FES | ETH_MACCR_DM;

    // Store-and-forward on both paths
    ETH->MTLTQOMR |= ETH_MTLTQOMR_TSF;
    ETH->MTLRQOMR |= ETH_MTLRQOMR_RSF;

    // DMA: contiguous descriptors (DSL = 0), moderate burst length
    ETH->DMACCR  = 0U;
    ETH->DMACTCR = (32UL << ETH_DMACTCR_TPBL_Pos);
    ETH->DMACRCR = (32UL << ETH_DMACRCR_RPBL_Pos)
                   | (static_cast<uint32_t>(BUFFER_SIZE) << ETH_DMACRCR_RBSZ_Pos);

    initTxDescriptors();
    initRxDescriptors();

    // Enable RX interrupt (TX descriptors are reclaimed in writeFrame)
    acknowledgeDmaInterrupts();
    ETH->DMACIER = ETH_DMACIER_NIE | ETH_DMACIER_RIE;

    return true;
}

void EthDriver::initRxDescriptors()
{
    auto const& descriptors = _config.rxDescriptors;
    size_t const count      = descriptors.size();

    ETL_ASSERT(
        _config.rxBuffers.size() >= (count * BUFFER_SIZE),
        ETL_ERROR_GENERIC("rx buffer pool too small"));
    ETL_ASSERT(_config.rxPbufs.size() == count, ETL_ERROR_GENERIC("rx pbuf count mismatch"));
    ETL_ASSERT(
        _config.rxPbufAtIndex.size() == count, ETL_ERROR_GENERIC("rx pbuf map count mismatch"));

    for (size_t i = 0U; i < count; ++i)
    {
        uint8_t* const buffer = &_config.rxBuffers[i * BUFFER_SIZE];
        pbuf* const p         = pbuf_alloced_custom(
            PBUF_RAW, BUFFER_SIZE, PBUF_REF, &_config.rxPbufs[i].buf, buffer, BUFFER_SIZE);
        ETL_ASSERT(p != nullptr, ETL_ERROR_GENERIC("pbuf must not be null"));
        _config.rxPbufs[i].driver                   = this;
        _config.rxPbufs[i].slot                     = &descriptors[i];
        _config.rxPbufs[i].buf.custom_free_function = &ethFreeCustomPbuf;
        _config.rxPbufAtIndex[i]                    = p;

        // Pointer-to-integer conversions are required to program the DMA.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        descriptors[i].des0 = reinterpret_cast<uint32_t>(buffer);
        descriptors[i].des1 = 0U;
        descriptors[i].des2 = 0U;
        descriptors[i].des3 = RX_DES3_OWN | RX_DES3_IOC | RX_DES3_BUF1V;
    }
    _rxNextFree = 0U;
    _rxNextBusy = 0U;

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    ETH->DMACRDLAR = reinterpret_cast<uint32_t>(descriptors.data());
    ETH->DMACRDRLR = static_cast<uint32_t>(count - 1U);
    ETH->DMACRDTPR = reinterpret_cast<uint32_t>(&descriptors[count - 1U]);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
}

void EthDriver::initTxDescriptors()
{
    auto const& descriptors = _config.txDescriptors;
    size_t const count      = descriptors.size();

    ETL_ASSERT(
        _config.txBuffers.size() >= (count * BUFFER_SIZE),
        ETL_ERROR_GENERIC("tx buffer pool too small"));

    for (size_t i = 0U; i < count; ++i)
    {
        descriptors[i].des0 = 0U;
        descriptors[i].des1 = 0U;
        descriptors[i].des2 = 0U;
        descriptors[i].des3 = 0U;
    }
    _txNext    = 0U;
    _txDirty   = 0U;
    _txPending = 0U;

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    ETH->DMACTDLAR = reinterpret_cast<uint32_t>(descriptors.data());
    ETH->DMACTDRLR = static_cast<uint32_t>(count - 1U);
    ETH->DMACTDTPR = reinterpret_cast<uint32_t>(descriptors.data());
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
}

void EthDriver::start()
{
    ETH->DMACTCR |= ETH_DMACTCR_ST;
    ETH->DMACRCR |= ETH_DMACRCR_SR;
    ETH->MACCR |= ETH_MACCR_TE | ETH_MACCR_RE;
}

void EthDriver::stop()
{
    ETH->MACCR &= ~(ETH_MACCR_TE | ETH_MACCR_RE);
    ETH->DMACTCR &= ~ETH_DMACTCR_ST;
    ETH->DMACRCR &= ~ETH_DMACRCR_SR;
}

void EthDriver::interrupt()
{
    acknowledgeDmaInterrupts();

    auto const& descriptors = _config.rxDescriptors;
    size_t const count      = descriptors.size();

    while (true)
    {
        size_t const appOwned = (_rxNextFree + count - _rxNextBusy) % count;
        if (appOwned >= (count - 1U))
        {
            break; // keep one descriptor as ring reserve (full/empty ambiguity)
        }
        uint32_t const des3 = descriptors[_rxNextFree].des3;
        if ((des3 & RX_DES3_OWN) != 0U)
        {
            break; // DMA still owns this descriptor
        }

        bool const complete = ((des3 & (RX_DES3_FD | RX_DES3_LD)) == (RX_DES3_FD | RX_DES3_LD));
        bool const error    = ((des3 & RX_DES3_ES) != 0U);
        uint16_t const length = static_cast<uint16_t>(des3 & RX_DES3_PL_MSK);

        pbuf* const p = _config.rxPbufAtIndex[_rxNextFree];

        if (complete && !error && (length > 0U) && !_queue.full())
        {
            // Hand the buffer to lwIP; the descriptor is re-armed when the
            // pbuf is freed (reclaimRxBuffer). Each pbuf permanently owns the
            // buffer it was created with at init time.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            auto* const customPbuf = reinterpret_cast<::lwiputils::RxCustomPbuf*>(p);
            size_t const pbufIndex = static_cast<size_t>(customPbuf - _config.rxPbufs.data());
            p->payload = &_config.rxBuffers[pbufIndex * BUFFER_SIZE];
            p->len     = length;
            p->tot_len = length;
            p->next    = nullptr;
            p->ref     = 1U;
            _queue.push(p);
            _rxNextFree = (_rxNextFree + 1U) % count;
        }
        else
        {
            // Dropped frame (error, fragment or full queue): re-arm the
            // oldest busy descriptor directly with this buffer.
            _rxNextFree = (_rxNextFree + 1U) % count;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            reclaimRxBuffer(reinterpret_cast<::lwiputils::RxCustomPbuf*>(p));
        }
    }
}

void EthDriver::reclaimRxBuffer(::lwiputils::RxCustomPbuf* const customPbuf)
{
    auto const& descriptors = _config.rxDescriptors;

    ::interrupts::SuspendResumeAllInterruptsScopedLock const lock;

    size_t const freedIndex
        = static_cast<size_t>(static_cast<EthDmaDescriptor*>(customPbuf->slot) - descriptors.data());
    size_t const rearmIndex = _rxNextBusy;

    // The freed buffer backs the oldest busy descriptor; swap the
    // pbuf-to-descriptor mapping accordingly. The buffer belonging to a pbuf
    // is fixed at init time and identified by the pbuf's index.
    size_t const pbufIndex     = static_cast<size_t>(customPbuf - _config.rxPbufs.data());
    uint8_t* const freedBuffer = &_config.rxBuffers[pbufIndex * BUFFER_SIZE];
    if (rearmIndex != freedIndex)
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* const other
            = reinterpret_cast<::lwiputils::RxCustomPbuf*>(_config.rxPbufAtIndex[rearmIndex]);
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
        ::ETL_OR_STD::swap(_config.rxPbufAtIndex[rearmIndex], _config.rxPbufAtIndex[freedIndex]);
        ::ETL_OR_STD::swap(other->slot, customPbuf->slot);
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    descriptors[rearmIndex].des0 = reinterpret_cast<uint32_t>(freedBuffer);
    descriptors[rearmIndex].des1 = 0U;
    descriptors[rearmIndex].des2 = 0U;
    __DMB();
    descriptors[rearmIndex].des3 = RX_DES3_OWN | RX_DES3_IOC | RX_DES3_BUF1V;
    __DMB();
    ETH->DMACRDTPR = reinterpret_cast<uint32_t>(&descriptors[rearmIndex]);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    _rxNextBusy = (rearmIndex + 1U) % descriptors.size();
}

void EthDriver::reclaimTxDescriptors()
{
    auto const& descriptors = _config.txDescriptors;
    while ((_txPending > 0U) && ((descriptors[_txDirty].des3 & TX_DES3_OWN) == 0U))
    {
        _txDirty = (_txDirty + 1U) % descriptors.size();
        --_txPending;
    }
}

bool EthDriver::writeFrame(netif* const /* aNetif */, pbuf const* const buf)
{
    if (buf == nullptr)
    {
        return false;
    }

    auto const& descriptors = _config.txDescriptors;
    size_t const count      = descriptors.size();

    reclaimTxDescriptors();
    if (_txPending >= count)
    {
        return false;
    }

    uint16_t const totalLength = buf->tot_len;
    if ((totalLength == 0U) || (totalLength > BUFFER_SIZE))
    {
        return false;
    }

    size_t const index    = _txNext;
    uint8_t* const buffer = &_config.txBuffers[index * BUFFER_SIZE];
    // pbuf_copy_partial takes a non-const pbuf pointer but does not modify it.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    (void)pbuf_copy_partial(const_cast<pbuf*>(buf), buffer, totalLength, 0U);

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    descriptors[index].des0 = reinterpret_cast<uint32_t>(buffer);
    descriptors[index].des1 = 0U;
    descriptors[index].des2 = static_cast<uint32_t>(totalLength) & TX_DES2_B1L_MSK;
    __DMB();
    descriptors[index].des3
        = TX_DES3_OWN | TX_DES3_FD | TX_DES3_LD | (static_cast<uint32_t>(totalLength) & 0x7FFFUL);
    __DMB();

    _txNext = (index + 1U) % count;
    ++_txPending;

    // Advance the tail pointer to the descriptor after the queued one; this
    // also wakes up a suspended TX DMA.
    ETH->DMACTDTPR = reinterpret_cast<uint32_t>(&descriptors[_txNext]);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    return true;
}

void EthDriver::setGroupcastAddressRecognition(::etl::array<uint8_t, 6> const /* mac */) const
{
    // Pass all multicast traffic: sufficient for the reference application
    // (SOME/IP service discovery, IGMP) and avoids hash filter maintenance.
    ETH->MACPFR |= ETH_MACPFR_PM;
}

bool EthDriver::phyWaitIdle() const
{
    uint32_t retries = BUSY_WAIT_LIMIT;
    while (((ETH->MACMDIOAR & ETH_MACMDIOAR_MB) != 0U) && (retries > 0U))
    {
        --retries;
    }
    return retries > 0U;
}

bool EthDriver::phyRead(uint8_t const reg, uint16_t& value) const
{
    if (!phyWaitIdle())
    {
        return false;
    }
    ETH->MACMDIOAR = (static_cast<uint32_t>(_config.phyAddress) << ETH_MACMDIOAR_PA_Pos)
                     | (static_cast<uint32_t>(reg) << ETH_MACMDIOAR_RDA_Pos)
                     | ETH_MACMDIOAR_CR_DIV102 | ETH_MACMDIOAR_MOC_RD | ETH_MACMDIOAR_MB;
    if (!phyWaitIdle())
    {
        return false;
    }
    value = static_cast<uint16_t>(ETH->MACMDIODR & 0xFFFFU);
    return true;
}

bool EthDriver::phyWrite(uint8_t const reg, uint16_t const value) const
{
    if (!phyWaitIdle())
    {
        return false;
    }
    ETH->MACMDIODR = value;
    ETH->MACMDIOAR = (static_cast<uint32_t>(_config.phyAddress) << ETH_MACMDIOAR_PA_Pos)
                     | (static_cast<uint32_t>(reg) << ETH_MACMDIOAR_RDA_Pos)
                     | ETH_MACMDIOAR_CR_DIV102 | ETH_MACMDIOAR_MOC_WR | ETH_MACMDIOAR_MB;
    return phyWaitIdle();
}

bool EthDriver::pollLink()
{
    uint16_t bmsr = 0U;
    if (!phyRead(PHY_REG_BMSR, bmsr))
    {
        _linkUp = false;
        return false;
    }
    bool const linkUp = ((bmsr & PHY_BMSR_LINK_UP) != 0U);

    if (linkUp && !_linkUp)
    {
        // Link came up: apply the negotiated speed/duplex to the MAC.
        uint16_t physcsr = 0U;
        if (phyRead(PHY_REG_PHYSCSR, physcsr))
        {
            uint16_t const speedIndication = static_cast<uint16_t>((physcsr >> 2) & 0x7U);
            uint32_t maccr                 = ETH->MACCR;
            maccr &= ~(ETH_MACCR_FES | ETH_MACCR_DM);
            if ((speedIndication & 0x2U) != 0U) // 100BASE-TX
            {
                maccr |= ETH_MACCR_FES;
            }
            if ((speedIndication & 0x4U) != 0U) // full duplex
            {
                maccr |= ETH_MACCR_DM;
            }
            ETH->MACCR = maccr;
        }
    }
    _linkUp = linkUp;
    return linkUp;
}

} // namespace ethernet
