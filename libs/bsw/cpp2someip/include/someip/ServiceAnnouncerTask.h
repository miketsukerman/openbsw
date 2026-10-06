/********************************************************************************
 * Copyright (c) 2026 Accenture
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#pragma once

#include "someip/ServiceDescription.h"

#include <ip/IPEndpoint.h>

#include <etl/intrusive_links.h>

namespace someip
{
class ServiceAnnouncerTask : public ::etl::forward_link<0>
{
public:
    enum class TaskType : uint8_t
    {
        TASK_SUBSCRIBE,
        TASK_SUBSCRIBE_ACK,
        TASK_SUBSCRIBE_ACK_MULTICAST,
        TASK_SUBSCRIBE_NACK,
        TASK_UNSUBSCRIBE,
        TASK_ANNOUNCE,
        TASK_IDLE
    };

    ServiceAnnouncerTask();

    ServiceAnnouncerTask& operator=(ServiceAnnouncerTask const& rhs);

    void clear();

    void init(
        service_id::type serviceId,
        instance_id::type instanceId,
        eventgroup_id::type eventgroup,
        uint32_t ttl,
        major_version::type majorVersion,
        minor_version::type minorVersion);

    void initFrom(
        ServiceDescription const& service,
        ::ip::IPAddress const& address,
        bool unicast,
        uint64_t timestamp,
        TaskType type);

    void setEndpoint(::ip::IPAddress const& address, uint16_t port);

    ServiceDescription const& getService() const;

    bool isSame(ServiceDescription const& service) const;

    ::ip::IPAddress const& getDestinationAddress() const;
    void setDestinationAddress(::ip::IPAddress const& address);

    uint64_t getTimestamp() const;
    void setTimestamp(uint64_t timestamp);

    void setMinorVersion(minor_version::type minorVersion);

    void setProto(uint8_t proto);

    TaskType getType() const;
    void setType(TaskType type);

    void setUnicast(bool unicast);

    bool containsEventGroup() const;

private:
    uint64_t _timestamp;
    ServiceDescription _service;
    ::ip::IPAddress _destination;

    TaskType _type;
    bool _unicast;
};

/*
 * inline implementation
 */
inline void ServiceAnnouncerTask::setMinorVersion(minor_version::type const minorVersion)
{
    _service.minorVersion = minorVersion;
}

inline void ServiceAnnouncerTask::setProto(uint8_t const proto) { _service.proto = proto; }

inline ServiceDescription const& ServiceAnnouncerTask::getService() const { return _service; }

inline ::ip::IPAddress const& ServiceAnnouncerTask::getDestinationAddress() const
{
    return _destination;
}

inline uint64_t ServiceAnnouncerTask::getTimestamp() const { return _timestamp; }

inline void ServiceAnnouncerTask::setTimestamp(uint64_t const timestamp) { _timestamp = timestamp; }

inline void ServiceAnnouncerTask::setDestinationAddress(::ip::IPAddress const& address)
{
    _destination = address;
}

inline ServiceAnnouncerTask::TaskType ServiceAnnouncerTask::getType() const { return _type; }

inline void ServiceAnnouncerTask::setType(TaskType const type) { _type = type; }

inline void ServiceAnnouncerTask::setUnicast(bool const unicast) { _unicast = unicast; }

} // namespace someip
