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

#include "etl/array.h"
#include "etl/span.h"
#include "lwipSocket/utils/LwipHelper.h"

namespace ethernet
{

/**
 * DMA descriptor of the STM32H7 Ethernet MAC (Synopsys DWC EQOS), normal
 * descriptor format, 4 words (DMACCR.DSL = 0, descriptors are contiguous).
 * Must live in non-cacheable memory reachable by the Ethernet DMA (D2 SRAM).
 */
struct EthDmaDescriptor
{
    volatile uint32_t des0;
    volatile uint32_t des1;
    volatile uint32_t des2;
    volatile uint32_t des3;
};

/**
 * Driver for the STM32H7 Ethernet MAC (ETH1) in RMII mode.
 *
 * - RX is zero-copy: each descriptor owns a full-frame sized buffer which is
 *   handed to lwIP as a custom pbuf; the descriptor is re-armed (with the
 *   buffer of the freed pbuf) once lwIP releases the pbuf.
 * - TX copies the outgoing pbuf chain into a driver-owned buffer so that lwIP
 *   payload memory (which lives in cached AXI SRAM) never has to be
 *   cache-maintained.
 * - All descriptors and buffers are expected to be placed in a non-cacheable
 *   MPU region (see configureMpuNonCacheable()); the driver then needs no
 *   cache maintenance at all.
 * - PHY access (generic IEEE 802.3 registers, suitable for the LAN8742) via
 *   the MAC's MDIO master.
 */
class EthDriver
{
public:
    EthDriver(EthDriver const&) = delete;

    /** Size of one RX/TX buffer; always holds a complete frame. */
    static constexpr uint16_t BUFFER_SIZE = 1536U;

    struct Configuration
    {
        uint8_t phyAddress;
        ::etl::span<EthDmaDescriptor> rxDescriptors;
        ::etl::span<EthDmaDescriptor> txDescriptors;
        /** rxDescriptors.size() * BUFFER_SIZE bytes, non-cacheable */
        ::etl::span<uint8_t> rxBuffers;
        /** txDescriptors.size() * BUFFER_SIZE bytes, non-cacheable */
        ::etl::span<uint8_t> txBuffers;
        ::etl::span<::lwiputils::RxCustomPbuf> rxPbufs;
        ::etl::span<pbuf*> rxPbufAtIndex;
    };

    EthDriver(::etl::array<uint8_t, 6> macAddr, Configuration const& configuration);

    /**
     * Configures an MPU region as normal, non-cacheable, non-executable
     * memory and invalidates any stale cache content for it. Must be called
     * before init() with the region containing descriptors and buffers.
     * \param baseAddress region base, aligned to the region size
     * \param sizeLog2 log2 of the region size in bytes (e.g. 15 for 32 KiB)
     * \param region MPU region number to use
     */
    static void configureMpuNonCacheable(uint32_t baseAddress, uint8_t sizeLog2, uint8_t region);

    /** Resets the MAC/DMA and configures descriptor rings; MAC still stopped. */
    bool init();
    /** Starts DMA + MAC transmitter/receiver. */
    void start();
    /** Stops DMA + MAC. */
    void stop();

    /** ETH interrupt service routine body: drains completed RX descriptors. */
    void interrupt();

    /** Copies the pbuf chain into a TX buffer and queues it for transmission. */
    bool writeFrame(netif* aNetif, pbuf const* buf);

    /** Accepts all multicast traffic (sufficient for the PoC use cases). */
    void setGroupcastAddressRecognition(::etl::array<uint8_t, 6> mac) const;

    /**
     * Polls the PHY via MDIO and reconfigures MAC speed/duplex on link-up.
     * \return true if the link is up
     */
    bool pollLink();

    bool phyRead(uint8_t reg, uint16_t& value) const;
    bool phyWrite(uint8_t reg, uint16_t value) const;

    /** Re-arms the oldest busy RX descriptor with the buffer of a freed pbuf. */
    void reclaimRxBuffer(::lwiputils::RxCustomPbuf* customPbuf);

    ::lwiputils::PbufQueue _queue;

private:
    void initRxDescriptors();
    void initTxDescriptors();
    void reclaimTxDescriptors();
    bool phyWaitIdle() const;

    ::etl::array<uint8_t, 6> const _macAddr;
    Configuration const _config;
    bool _linkUp       = false;
    size_t _rxNextFree = 0; ///< next descriptor the DMA will complete
    size_t _rxNextBusy = 0; ///< oldest descriptor waiting to be re-armed
    size_t _txNext     = 0; ///< next TX descriptor to use
    size_t _txDirty    = 0; ///< oldest TX descriptor not yet reclaimed
    size_t _txPending  = 0; ///< number of TX descriptors owned by the DMA
};

} // namespace ethernet
