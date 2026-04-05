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

WHEN(R"(the client sends a begin upgrade command with firmware size {int})", (std::int32_t firmwareSize))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnBeginUpgrade(static_cast<uint32_t>(firmwareSize)));
    fixture.fwuClient->SendBeginUpgrade(fixture.config.nodeId, static_cast<uint32_t>(firmwareSize));
}

THEN(R"(the firmware upgrade server observer shall have received a begin upgrade with size {int})", (std::int32_t))
{
    SUCCEED();
}

WHEN(R"(the client sends data block {int})", (std::int32_t blockIndex))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnDataBlock(static_cast<uint16_t>(blockIndex), _));
    hal::Can::Message blockData;
    blockData.push_back(0xAA);
    fixture.fwuClient->SendDataBlock(fixture.config.nodeId, static_cast<uint16_t>(blockIndex), blockData);
}

WHEN(R"(the client sends an abort command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnAbort());
    fixture.fwuClient->SendAbort(fixture.config.nodeId);
}

WHEN(R"(the client sends a verify command with CRC32 {int})", (std::int32_t crc32))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnVerify(static_cast<uint32_t>(crc32)));
    fixture.fwuClient->SendVerify(fixture.config.nodeId, static_cast<uint32_t>(crc32));
}

WHEN(R"(the client sends a query progress command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnQueryProgress());
    fixture.fwuClient->SendQueryProgress(fixture.config.nodeId);
}

WHEN(R"({int} seconds elapse expecting a session timeout)", (std::int32_t seconds))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.fwuServerObserver, OnSessionTimeout());
    fixture.ForwardTime(std::chrono::seconds(seconds));
}

WHEN(R"({int} seconds elapse without a session timeout)", (std::int32_t seconds))
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.ForwardTime(std::chrono::seconds(seconds));
}

THEN(R"(the firmware upgrade server observer shall have received a query progress notification)")
{
    SUCCEED();
}
