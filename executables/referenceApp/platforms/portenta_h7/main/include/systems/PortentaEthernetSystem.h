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

#include "ethernet/EthDriver.h"
#include "systems/IEthernetDriverSystem.h"

#include <async/Async.h>
#include <async/util/MemberCall.h>
#include <lifecycle/SingleContextLifecycleComponent.h>

namespace systems
{

/**
 * Ethernet driver system for the Arduino Portenta H7.
 *
 * Brings up the on-module LAN8742AI PHY (RMII, RJ45 via the Portenta Hat
 * Carrier) and the STM32H747 Ethernet MAC. DMA descriptors and frame buffers
 * live in D2 SRAM3, configured non-cacheable through the MPU, so no cache
 * maintenance is needed despite the enabled D-cache.
 */
class PortentaEthernetSystem final
: public ::ethernet::IEthernetDriverSystem
, public ::lifecycle::SingleContextLifecycleComponent
{
public:
    explicit PortentaEthernetSystem(::async::ContextType context);

    void init() override;
    void run() override;
    void shutdown() override;

    void setGroupcastAddressRecognition(::etl::array<uint8_t, 6> const mac) const override
    {
        _driver.setGroupcastAddressRecognition(mac);
    }

    bool getLinkStatus(size_t port) override;

    bool writeFrame(struct netif* const aNetif, struct pbuf* const buf) override
    {
        return _driver.writeFrame(aNetif, buf);
    }

    ::lwiputils::PbufQueue& getRx() override { return _driver._queue; }

private:
    void pollLink();

    ::async::ContextType _context;
    ::async::MemberCall<PortentaEthernetSystem, &PortentaEthernetSystem::pollLink> _pollRunnable;
    ::async::TimeoutType _pollTimeout;
    ::ethernet::EthDriver::Configuration _driverConfig;
    ::ethernet::EthDriver _driver;
    bool _initialized = false;
    bool _linkUp      = false;
};

} // namespace systems
