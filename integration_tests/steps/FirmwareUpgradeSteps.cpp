#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;
using namespace services;
using integration::ApplicationFixture;

GIVEN(R"(the firmware upgrade category is registered on both client and server)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.RegisterFirmwareUpgrade();
}

namespace
{
    void ExpectBeginResponse(ApplicationFixture& fixture)
    {
        EXPECT_CALL(*fixture.fwuClientObserver, OnBeginResponse(_, _)).WillOnce([&fixture](FwuError status, uint16_t)
            {
                fixture.fwuBeginResponseStatus = status;
            });
    }

    void SendBeginUpgradeAnsweredWith(ApplicationFixture& fixture, uint32_t firmwareSize, FwuError status)
    {
        EXPECT_CALL(*fixture.fwuServerObserver, OnBeginUpgrade(_, _)).WillOnce([&fixture, status](uint32_t size, const infra::Function<void(FwuError, uint16_t)>& onResult)
            {
                fixture.fwuBeginSize = size;
                onResult(status, 4096);
            });
        ExpectBeginResponse(fixture);
        fixture.fwuClient->SendBeginUpgrade(fixture.config.nodeId, firmwareSize);
    }
}

GIVEN(R"(the server application has detached from the firmware upgrade category)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.fwuServerObserver.reset();
}

WHEN(R"(the client sends a begin upgrade command with firmware size {int})", (std::int32_t firmwareSize))
{
    SendBeginUpgradeAnsweredWith(context.Get<ApplicationFixture>(), static_cast<uint32_t>(firmwareSize), FwuError::ok);
}

WHEN(R"(the client sends a begin upgrade command that the server refuses as busy)")
{
    SendBeginUpgradeAnsweredWith(context.Get<ApplicationFixture>(), 1024, FwuError::busy);
}

WHEN(R"(the client sends a begin upgrade command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    ExpectBeginResponse(fixture);
    fixture.fwuClient->SendBeginUpgrade(fixture.config.nodeId, 1024);
}

THEN(R"(the client shall have received a begin upgrade response with status ok)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_TRUE(fixture.fwuBeginResponseStatus.has_value());
    EXPECT_EQ(*fixture.fwuBeginResponseStatus, FwuError::ok);
}

THEN(R"(the client shall have received a begin upgrade response with status busy)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_TRUE(fixture.fwuBeginResponseStatus.has_value());
    EXPECT_EQ(*fixture.fwuBeginResponseStatus, FwuError::busy);
}

THEN(R"(the client shall have received a begin upgrade response with status not ready)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_TRUE(fixture.fwuBeginResponseStatus.has_value());
    EXPECT_EQ(*fixture.fwuBeginResponseStatus, FwuError::notReady);
}

THEN(R"(the firmware upgrade server observer shall have received a begin upgrade with size {int})", (std::int32_t firmwareSize))
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_TRUE(fixture.fwuBeginSize.has_value());
    EXPECT_EQ(*fixture.fwuBeginSize, static_cast<uint32_t>(firmwareSize));
}

WHEN(R"(the client sends data block {int})", (std::int32_t blockIndex))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnDataBlock(static_cast<uint16_t>(blockIndex), _, _)).WillOnce([](uint16_t, infra::ConstByteRange, const infra::Function<void(FwuError)>& onResult)
        {
            onResult(FwuError::ok);
        });
    EXPECT_CALL(*fixture.fwuClientObserver, OnDataBlockAck(FwuError::ok, static_cast<uint16_t>(blockIndex)));
    hal::Can::Message blockData;
    blockData.push_back(0xAA);
    fixture.fwuClient->SendDataBlock(fixture.config.nodeId, static_cast<uint16_t>(blockIndex), blockData);
}

WHEN(R"(the client sends an abort command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnAbort(_))
        .WillOnce(Invoke([](const infra::Function<void()>& onDone)
            {
                onDone();
            }));
    fixture.fwuClient->SendAbort(fixture.config.nodeId);
}

WHEN(R"(the client sends a verify command with CRC32 {int})", (std::int32_t crc32))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnVerify(static_cast<uint32_t>(crc32), _));
    fixture.fwuClient->SendVerify(fixture.config.nodeId, static_cast<uint32_t>(crc32));
}

WHEN(R"(the client sends a query progress command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnQueryProgress(_)).WillOnce([&fixture](const infra::Function<void(FwuState, uint16_t, uint16_t)>&)
        {
            fixture.fwuProgressQueried = true;
        });
    fixture.fwuClient->SendQueryProgress(fixture.config.nodeId);
}

THEN(R"(the firmware upgrade server observer shall have received a query progress notification)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_TRUE(fixture.fwuProgressQueried);
}

THEN(R"(the firmware upgrade session shall have timed out)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_TRUE(fixture.fwuSessionTimedOut);
}

THEN(R"(the firmware upgrade session shall not have timed out)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_FALSE(fixture.fwuSessionTimedOut);
}
