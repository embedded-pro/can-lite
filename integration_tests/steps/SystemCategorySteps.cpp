#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gtest/gtest.h"

using namespace services;
using integration::ApplicationFixture;

WHEN(R"({int} second elapses)", (std::int32_t seconds))
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.ForwardTime(std::chrono::seconds(seconds));
}

THEN(R"(the server shall have transmitted a heartbeat frame)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_GT(fixture.serverCan.sendCount, 0);

    uint32_t rawId = fixture.serverCan.lastSentId.Get29BitId();
    EXPECT_EQ(ExtractCanMessageType(rawId), canHeartbeatMessageTypeId);
}

THEN(R"(the heartbeat frame shall contain protocol version {int})", (std::int32_t version))
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_GE(fixture.serverCan.lastSentData.size(), 1u);
    EXPECT_EQ(fixture.serverCan.lastSentData[0], static_cast<uint8_t>(version));
}

WHEN(R"(a heartbeat is sent by the server after {int} second)", (std::int32_t seconds))
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.ForwardTime(std::chrono::seconds(seconds));
}

THEN(R"(the heartbeat payload byte 0 shall equal {int})", (std::int32_t value))
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_GE(fixture.serverCan.lastSentData.size(), 1u);
    EXPECT_EQ(fixture.serverCan.lastSentData[0], static_cast<uint8_t>(value));
}

WHEN(R"(the client sends a command with category {int} and unknown message type {int} to the server)", (std::int32_t category, std::int32_t msgType))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::command, static_cast<uint8_t>(category),
        static_cast<uint8_t>(msgType), fixture.config.nodeId);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;

    fixture.serverCan.InjectFrame(id, msg);
}

THEN(R"(the server shall send an acknowledgement with status {string})", (const std::string& statusStr))
{
    auto& fixture = context.Get<ApplicationFixture>();

    ASSERT_GE(fixture.serverCan.lastSentData.size(), 3u);

    uint32_t rawId = fixture.serverCan.lastSentId.Get29BitId();
    EXPECT_EQ(ExtractCanMessageType(rawId), canCommandAckMessageTypeId);

    auto expectedStatus = CanAckStatus::success;
    if (statusStr == "unknownCommand")
        expectedStatus = CanAckStatus::unknownCommand;
    else if (statusStr == "invalidPayload")
        expectedStatus = CanAckStatus::invalidPayload;
    else if (statusStr == "invalidState")
        expectedStatus = CanAckStatus::invalidState;
    else if (statusStr == "sequenceError")
        expectedStatus = CanAckStatus::sequenceError;
    else if (statusStr == "rateLimited")
        expectedStatus = CanAckStatus::rateLimited;

    EXPECT_EQ(static_cast<CanAckStatus>(fixture.serverCan.lastSentData[2]), expectedStatus);
}

WHEN(R"(the client sends a status request to the server)")
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::command, canSystemCategoryId,
        canStatusRequestMessageTypeId, fixture.config.nodeId);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;

    fixture.serverCan.InjectFrame(id, msg);
}

THEN(R"(the server shall respond with a heartbeat message)")
{
    auto& fixture = context.Get<ApplicationFixture>();

    ASSERT_GT(fixture.serverCan.sendCount, 0);

    uint32_t rawId = fixture.serverCan.lastSentId.Get29BitId();
    EXPECT_EQ(ExtractCanCategory(rawId), canSystemCategoryId);
    EXPECT_EQ(ExtractCanMessageType(rawId), canHeartbeatMessageTypeId);
    ASSERT_GE(fixture.serverCan.lastSentData.size(), 1u);
    EXPECT_EQ(fixture.serverCan.lastSentData[0], canProtocolVersion);
}
