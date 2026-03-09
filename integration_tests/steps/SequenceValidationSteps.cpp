#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gtest/gtest.h"

using namespace services;
using integration::ApplicationFixture;

GIVEN(R"(a sequenced test category with ID {int} is registered on the server)", (std::int32_t categoryId))
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.RegisterSequencedCategory(static_cast<uint8_t>(categoryId));
}

WHEN(R"(the client sends a command to category {int} with sequence number {int})", (std::int32_t category, std::int32_t seqNum))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::command, static_cast<uint8_t>(category),
        0x01, fixture.config.nodeId);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;
    msg.push_back(static_cast<uint8_t>(seqNum));

    fixture.serverCan.InjectFrame(id, msg);
}

THEN(R"(the server category handler shall have received {int} command)", (std::int32_t count))
{
    auto& fixture = context.Get<ApplicationFixture>();
    auto* category = fixture.FindSequencedCategory(3);
    ASSERT_NE(category, nullptr);
    EXPECT_EQ(category->msg.handleCount, count);
}

THEN(R"(the server category handler shall have received {int} commands)", (std::int32_t count))
{
    auto& fixture = context.Get<ApplicationFixture>();
    auto* category = fixture.FindSequencedCategory(3);
    ASSERT_NE(category, nullptr);
    EXPECT_EQ(category->msg.handleCount, count);
}

WHEN(R"(the client sends a status request to the server with empty payload)")
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::command, canSystemCategoryId,
        canStatusRequestMessageTypeId, fixture.config.nodeId);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;

    fixture.serverCan.InjectFrame(id, msg);
}

WHEN(R"(the client sends a command to category {int} with empty payload)", (std::int32_t category))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::command, static_cast<uint8_t>(category),
        0x01, fixture.config.nodeId);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;

    fixture.serverCan.InjectFrame(id, msg);
}
