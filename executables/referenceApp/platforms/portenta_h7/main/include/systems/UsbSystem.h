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

#include <lifecycle/SingleContextLifecycleComponent.h>

#include <async/Async.h>
#include <async/util/MemberCall.h>

namespace systems
{

/**
 * Lifecycle component servicing the USB-CDC console device.
 *
 * run(): initializes the USB device stack (clocks, PHY, TinyUSB) and
 * schedules a 1 ms periodic runnable that calls the TinyUSB device task.
 * The OTG_HS interrupt only queues events; all processing happens in the
 * periodic poll from task context.
 */
class UsbSystem final : public ::lifecycle::SingleContextLifecycleComponent
{
public:
    explicit UsbSystem(::async::ContextType context);

    void init() override;
    void run() override;
    void shutdown() override;

private:
    void poll();

    ::async::ContextType _context;
    ::async::MemberCall<UsbSystem, &UsbSystem::poll> _pollRunnable;
    ::async::TimeoutType _pollTimeout;
};

} // namespace systems
