#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "support/ResultTypes.hpp"
#include "gtest/gtest.h"

using namespace services;
using integration::ApplicationFixture;

WHEN(R"(a frame is received with an 11-bit standard identifier)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    auto id = hal::Can::Id::Create11BitId(0x100);
    hal::Can::Message msg;
    msg.push_back(0x01);

    fixture.serverCan.InjectFrame(id, msg);
}

THEN(R"(the server shall silently discard the frame)")
{
    SUCCEED();
}

WHEN(R"(the client receives a frame with an 11-bit standard identifier)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    auto id = hal::Can::Id::Create11BitId(0x100);
    hal::Can::Message msg;
    msg.push_back(0x01);

    fixture.clientCan.InjectFrame(id, msg);
}

THEN(R"(the client shall silently discard the frame)")
{
    SUCCEED();
}

WHEN(R"(a CAN identifier is constructed with priority {int}, category {int}, message type {int}, and node ID {int})",
    (std::int32_t priority, std::int32_t category, std::int32_t messageType, std::int32_t nodeId))
{
    auto result = context.Emplace<integration::CanIdResult>();
    result->rawId = MakeCanId(static_cast<CanPriority>(priority),
        static_cast<uint8_t>(category),
        static_cast<uint8_t>(messageType),
        static_cast<uint16_t>(nodeId));
}

THEN(R"(the 29-bit identifier shall encode priority in bits 28-24)")
{
    auto& result = context.Get<integration::CanIdResult>();
    EXPECT_EQ(ExtractCanPriority(result.rawId), static_cast<CanPriority>(4));
}

THEN(R"(the 29-bit identifier shall encode category in bits 23-20)")
{
    auto& result = context.Get<integration::CanIdResult>();
    EXPECT_EQ(ExtractCanCategory(result.rawId), 2u);
}

THEN(R"(the 29-bit identifier shall encode message type in bits 19-12)")
{
    auto& result = context.Get<integration::CanIdResult>();
    EXPECT_EQ(ExtractCanMessageType(result.rawId), 1u);
}

THEN(R"(the 29-bit identifier shall encode node ID in bits 11-0)")
{
    auto& result = context.Get<integration::CanIdResult>();
    EXPECT_EQ(ExtractCanNodeId(result.rawId), 42u);
}

WHEN(R"(a frame is sent addressed to node {int})", (std::int32_t nodeId))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::heartbeat, canSystemCategoryId,
        canHeartbeatMessageTypeId, static_cast<uint16_t>(nodeId));
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;
    msg.push_back(canProtocolVersion);

    fixture.serverCan.InjectFrame(id, msg);
}

WHEN(R"(a heartbeat frame is sent addressed to the broadcast address {int})", (std::int32_t nodeId))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::heartbeat, canSystemCategoryId,
        canHeartbeatMessageTypeId, static_cast<uint16_t>(nodeId));
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;
    msg.push_back(canProtocolVersion);

    fixture.serverCan.InjectFrame(id, msg);
}

THEN(R"(the server shall process the broadcast heartbeat)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_GT(fixture.processedCount, 0);
}

WHEN(R"(a frame is received with unregistered category {int})", (std::int32_t category))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::command, static_cast<uint8_t>(category),
        0x01, fixture.config.nodeId);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;
    msg.push_back(0x01);

    fixture.serverCan.InjectFrame(id, msg);
}

WHEN(R"(a frame is received with system category and unknown message type {int})", (std::int32_t msgType))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::heartbeat, canSystemCategoryId,
        static_cast<uint8_t>(msgType), fixture.config.nodeId);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;

    fixture.serverCan.InjectFrame(id, msg);
}

GIVEN(R"(the FOC motor category is registered on the server)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.RegisterFocMotorServerOnly();
}

WHEN(R"(the client sends a set PID current command with only {int} bytes)", (std::int32_t byteCount))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::command, focMotorCategoryId,
        focSetPidCurrentId, fixture.config.nodeId);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;
    for (std::int32_t i = 0; i < byteCount; ++i)
        msg.push_back(static_cast<uint8_t>(i));

    fixture.serverCan.InjectFrame(id, msg);
}

THEN(R"(the motor server observer shall not have received a PID current command)")
{
    SUCCEED();
}

WHEN(R"(the client receives a response for unregistered category {int})", (std::int32_t category))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::response, static_cast<uint8_t>(category),
        0x01, 0);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;
    msg.push_back(0xAA);

    fixture.clientCan.InjectFrame(id, msg);
}
