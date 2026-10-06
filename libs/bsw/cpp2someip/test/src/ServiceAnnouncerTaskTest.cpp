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
#include "someip/SomeIpConstants.h"
#include "someip/init.h"

#include <ip/IPAddress.h>
#include <gtest/gtest.h>

namespace
{
using namespace ::someip;

/**
 * Make sure isSame() detects correctly whether combination of serviceId, instanceId,
 * minorVersion and MajorVersion of service and task match.
 */
TEST(ServiceAnnouncerTask, test_IsSame)
{
    ServiceAnnouncerTask task;
    task.init(10U, 11U, 12U, 13U, 14U, 15U);
    auto service = ::someip::make<ServiceDescription>();

    service.serviceId = 9U;
    EXPECT_FALSE(task.isSame(service));

    service.serviceId = 10U;
    EXPECT_FALSE(task.isSame(service));

    service.instanceId = 11U;
    EXPECT_FALSE(task.isSame(service));

    service.eventGroup   = 12U;
    service.majorVersion = 14U;
    service.minorVersion = 15U;
    EXPECT_TRUE(task.isSame(service));

    service.eventGroup = 13U;
    EXPECT_FALSE(task.isSame(service));
    service.eventGroup = ::someip::eventgroup_id::ALL;
    EXPECT_TRUE(task.isSame(service));
    service.eventGroup = 12U;

    service.instanceId = 10U;
    EXPECT_FALSE(task.isSame(service));
    task.init(10U, ::someip::instance_id::ANY, 12U, 13U, 14U, 15U);
    EXPECT_TRUE(task.isSame(service));

    service.majorVersion = 13U;
    service.minorVersion = 15U;
    service.instanceId   = 11U;
    EXPECT_FALSE(task.isSame(service));
    task.init(10U, 11U, 12U, 13U, major_version::ANY, 15U);
    EXPECT_TRUE(task.isSame(service));

    service.majorVersion = 14U;
    service.minorVersion = 13U;
    service.instanceId   = 11U;
    EXPECT_FALSE(task.isSame(service));
    task.init(10U, 11U, 12U, 13U, 14U, minor_version::ANY);
    EXPECT_TRUE(task.isSame(service));

    service.majorVersion = 13U;
    service.minorVersion = 13U;
    service.instanceId   = 11U;
    EXPECT_FALSE(task.isSame(service));
    task.init(10U, 11U, 12U, 13U, major_version::ANY, minor_version::ANY);
    EXPECT_TRUE(task.isSame(service));
}

/**
 * Make sure timestamp of ServiceAnnouncerTask is set and given back consistently.
 */
TEST(ServiceAnnouncerTask, set_and_get_timestamp)
{
    ServiceAnnouncerTask task;
    task.setTimestamp(13U);
    EXPECT_EQ(13U, task.getTimestamp());
}

/**
 * Make sure ServiceAnnouncerTask is assigned correctly.
 */
TEST(ServiceAnnouncerTask, test_ServiceAnnouncerTask_assignment)
{
    ServiceAnnouncerTask task1;
    ServiceAnnouncerTask task2;

    task1.init(10U, 11U, 12U, 13U, 14U, 15U);
    task1.setEndpoint(::ip::make_ip4(192U, 168U, 10U, 1U), 2222U);
    task1.setProto(6U);
    task2.init(11U, 12U, 13U, 14U, 15U, 16U);
    task2.setEndpoint(::ip::make_ip4(192U, 168U, 20U, 1U), 3333U);
    task2.setProto(17U);

    ServiceAnnouncerTask* other = &task1;
    task1                       = *other;
    EXPECT_EQ(10U, task1.getService().serviceId);
    EXPECT_EQ(11U, task1.getService().instanceId);
    EXPECT_EQ(12U, task1.getService().eventGroup);
    EXPECT_EQ(13U, task1.getService().ttl);
    EXPECT_EQ(14U, task1.getService().majorVersion);
    EXPECT_EQ(15U, task1.getService().minorVersion);
    EXPECT_EQ(::ip::make_ip4(192U, 168U, 10U, 1U), task1.getService().ipAddress);
    EXPECT_EQ(2222U, task1.getService().port);
    EXPECT_EQ(6U, task1.getService().proto);

    task1 = task2;
    EXPECT_EQ(11U, task1.getService().serviceId);
    EXPECT_EQ(12U, task1.getService().instanceId);
    EXPECT_EQ(13U, task1.getService().eventGroup);
    EXPECT_EQ(14U, task1.getService().ttl);
    EXPECT_EQ(15U, task1.getService().majorVersion);
    EXPECT_EQ(16U, task1.getService().minorVersion);
    EXPECT_EQ(::ip::make_ip4(192U, 168U, 20U, 1U), task1.getService().ipAddress);
    EXPECT_EQ(3333U, task1.getService().port);
    EXPECT_EQ(17U, task1.getService().proto);
}

TEST(ServiceAnnouncerTask, test_initFrom_copies_service_payload)
{
    ServiceAnnouncerTask task;

    auto source             = ::someip::make<ServiceDescription>();
    source.serviceId        = 1U;
    source.instanceId       = 2U;
    source.eventGroup       = 3U;
    source.majorVersion     = 4U;
    source.minorVersion     = 5U;
    source.ttl              = 6U;
    source.ipAddress        = ::ip::make_ip4(10U, 0U, 0U, 42U);
    source.port             = 7U;
    source.proto            = 8U;
    ::ip::IPAddress const destination = ::ip::make_ip4(10U, 0U, 0U, 99U);

    task.initFrom(
        source,
        destination,
        true,
        1234U,
        ::someip::ServiceAnnouncerTask::TaskType::TASK_SUBSCRIBE_ACK_MULTICAST);

    ServiceDescription const& copied = task.getService();
    EXPECT_EQ(source.serviceId, copied.serviceId);
    EXPECT_EQ(source.instanceId, copied.instanceId);
    EXPECT_EQ(source.eventGroup, copied.eventGroup);
    EXPECT_EQ(source.majorVersion, copied.majorVersion);
    EXPECT_EQ(source.minorVersion, copied.minorVersion);
    EXPECT_EQ(source.ttl, copied.ttl);
    EXPECT_EQ(source.ipAddress, copied.ipAddress);
    EXPECT_EQ(source.port, copied.port);
    EXPECT_EQ(source.proto, copied.proto);
    EXPECT_EQ(destination, task.getDestinationAddress());
    EXPECT_EQ(1234U, task.getTimestamp());
}
} // anonymous namespace
