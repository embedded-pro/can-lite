#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gtest/gtest.h"

using namespace services;
using integration::ApplicationFixture;

WHEN(R"(the client sends {int} heartbeat messages to the server)", (std::int32_t count))
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::heartbeat, canSystemCategoryId,
        canHeartbeatMessageTypeId, fixture.config.nodeId);
    auto id = hal::Can::Id::Create29BitId(rawId);

    for (std::int32_t i = 0; i < count; ++i)
    {
        hal::Can::Message msg;
        msg.push_back(canProtocolVersion);
        fixture.serverCan.InjectFrame(id, msg);
    }
}

THEN(R"(the server shall have processed {int} messages)", (std::int32_t expected))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_EQ(fixture.processedCount, expected);
}
