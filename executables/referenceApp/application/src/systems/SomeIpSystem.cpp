/********************************************************************************
 * Copyright (c) 2025 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "systems/SomeIpSystem.h"

#include "async/Types.h"
#include "ethConfig.h"
#include "ip/IPAddress.h"
#include "someip/ServiceDescription.h"
#include "someip/SomeIpConstants.h"

#include <etl/tuple.h>

etl::tuple<uint16_t, ::someip::PortRangeReturnCode>
computeNextLocalPort(uint16_t const requestedPort, uint16_t const)
{
    return etl::make_tuple(requestedPort, ::someip::PortRangeReturnCode::OK);
}

namespace systems
{

namespace
{

static constexpr auto multicastIp     = ::ip::make_ip4(225, 0, 0, 1);
static constexpr auto remoteServiceIp = ::ip::make_ip4(192, 168, 0, 20);
static constexpr uint16_t providedServiceId   = 0xCAFEU;
static constexpr uint16_t consumedServiceId   = 0xBABEU;
static constexpr uint16_t defaultInstanceId   = 1U;
static constexpr uint8_t defaultMajorVersion  = 1U;
static constexpr uint32_t providedServiceTtl  = 10U;
static constexpr uint32_t consumedServiceTtl  = 1U;
static constexpr uint16_t exampleEventGroupId = 0x8001U;
static constexpr uint16_t servicePort         = 30501U;
static constexpr uint16_t clientPort          = 30502U;

static ::someip::ServiceDescription const providedEventGroupDescription{
    0U,
    providedServiceTtl,
    providedServiceId,
    defaultInstanceId,
    exampleEventGroupId,
    ::eth0::IP_ADDRESS,
    servicePort,
    ::someip::proto::SD_L4_PROTO_UDP,
    defaultMajorVersion};

static ::someip::ServiceDescription const providedServiceDescription{
    0U,
    providedServiceTtl,
    providedServiceId,
    defaultInstanceId,
    ::someip::eventgroup_id::ALL,
    ::eth0::IP_ADDRESS,
    servicePort,
    ::someip::proto::SD_L4_PROTO_UDP,
    defaultMajorVersion};

static ::someip::ServiceDescription const consumedEventGroupDescription{
    0U,
    consumedServiceTtl,
    consumedServiceId,
    defaultInstanceId,
    exampleEventGroupId,
    remoteServiceIp,
    clientPort,
    ::someip::proto::SD_L4_PROTO_UDP,
    defaultMajorVersion};

static ::someip::ServiceDescription const consumedServiceDescription{
    0U,
    consumedServiceTtl,
    consumedServiceId,
    defaultInstanceId,
    ::someip::eventgroup_id::ALL,
    remoteServiceIp,
    clientPort,
    ::someip::proto::SD_L4_PROTO_UDP,
    defaultMajorVersion};

} // namespace

SomeIpSystem::SomeIpSystem(::async::ContextType const context)
: _timeout()
, _context(context)
, _stack(multicastIp, ::eth0::IP_ADDRESS, uint8_t{}, _context)
, _methodCallbacks()
, _providedService()
, _providedServiceHandler(_methodCallbacks, _stack.getEventTransceiver())
, _eventListener()
, _consumedServiceQuery()
, _rpcChannel(_stack.getNetwork(), _stack.getRpcHandler())
, _serviceListener(_rpcChannel)
, _routine(_rpcChannel)
{}

void SomeIpSystem::init()
{
    _stack.initSdPort(SD_PORT);
    _stack.initUdpPort(SERVICE_PORT);
    _stack.initUdpPort(CLIENT_PORT);
    _stack.init();
    _stack.addEventListener(_eventListener);

    /*
     * Register the service {{
     */
    _providedServiceEg.setHandler(_providedServiceHandler);
    _providedServiceEg.description = providedEventGroupDescription;
    _stack.registerProvidedService(_providedServiceEg);

    _providedService.setHandler(_providedServiceHandler);
    _providedService.description = providedServiceDescription;
    _stack.registerProvidedService(_providedService);

    /*
     * }} register the service.
     */

    /*
     * Register the client {{
     */

    _consumedServiceQueryEg.description = consumedEventGroupDescription;
    _consumedServiceQueryEg.listener    = &_serviceListener;
    _stack.registerServiceQuery(_consumedServiceQueryEg);

    _consumedServiceQuery.description = consumedServiceDescription;
    _consumedServiceQuery.listener    = &_serviceListener;
    _stack.registerServiceQuery(_consumedServiceQuery);

    /*
     * }} register the client
     */

    _stack.start();
    transitionDone();
}

void SomeIpSystem::run()
{
    ::async::scheduleAtFixedRate(
        _context, _routine, _timeout, 100, ::async::TimeUnit::MILLISECONDS);

    transitionDone();
}

void SomeIpSystem::shutdown()
{
    _stack.shutdown();
    transitionDone();
}

} // namespace systems
