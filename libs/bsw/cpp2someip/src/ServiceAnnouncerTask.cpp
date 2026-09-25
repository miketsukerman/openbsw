/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "someip/ServiceAnnouncerTask.h"

#include "someip/ServiceDescription.h"
#include "someip/init.h"

namespace someip
{
ServiceAnnouncerTask::ServiceAnnouncerTask()
: ::etl::forward_link<0>()
, _timestamp()
, _service()
, _destination()
, _type()
, _unicast()
{
    clear();
}

void ServiceAnnouncerTask::clear()
{
    _timestamp = 0U;
    _service   = make<ServiceDescription>();
#ifdef PLATFORM_SUPPORT_IPV6
    _destination = ::ip::make_ip6(0U);
#else
    _destination = ::ip::make_ip4(0U);
#endif
    _type         = TaskType::TASK_IDLE;
    _unicast      = false;
}

ServiceAnnouncerTask& ServiceAnnouncerTask::operator=(ServiceAnnouncerTask const& rhs)
{
    if (this != &rhs)
    {
        _timestamp   = rhs._timestamp;
        _service     = rhs._service;
        _destination = rhs._destination;
        _type        = rhs._type;
        _unicast     = rhs._unicast;
    }

    return *this;
}

void ServiceAnnouncerTask::init(
    service_id::type const serviceId,
    instance_id::type const instanceId,
    eventgroup_id::type const eventgroup,
    ttl::type const ttl,
    major_version::type const majorVersion,
    minor_version::type const minorVersion)
{
    _service.serviceId    = serviceId;
    _service.instanceId   = instanceId;
    _service.eventGroup   = eventgroup;
    _service.ttl          = ttl;
    _service.majorVersion = majorVersion;
    _service.minorVersion = minorVersion;
}

void ServiceAnnouncerTask::setEndpoint(::ip::IPAddress const& address, uint16_t const port)
{
    _service.ipAddress = address;
    _service.port      = port;
}

void ServiceAnnouncerTask::initFrom(
    ServiceDescription const& service,
    ::ip::IPAddress const& address,
    bool const unicast,
    uint64_t const timestamp,
    TaskType const type)
{
    _service   = service;
    _destination  = address;
    _unicast   = unicast;
    _timestamp = timestamp;
    _type      = type;
}

bool ServiceAnnouncerTask::isSame(ServiceDescription const& service) const
{
    ServiceKey const key = getServiceKey(_service);
    bool const majorVersionOk
        = ((key.majorVersion == major_version::ANY) || (key.majorVersion == service.majorVersion));
    bool const minorVersionOk
        = ((_service.minorVersion == minor_version::ANY)
           || (_service.minorVersion == service.minorVersion));

    return (
        (key.serviceId == service.serviceId)
        && ((key.instanceId == service.instanceId) || (key.instanceId == instance_id::ANY))
        && majorVersionOk && minorVersionOk);
}

bool ServiceAnnouncerTask::containsEventGroup() const
{
    return (_service.eventGroup != eventgroup_id::ALL);
}

} // namespace someip
