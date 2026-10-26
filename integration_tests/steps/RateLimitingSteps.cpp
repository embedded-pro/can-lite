#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gtest/gtest.h"

using namespace services;
using integration::ApplicationFixture;

namespace
{
    void InjectHeartbeats(ApplicationFixture& fixture, std::int32_t count)
    {
        uint32_t rawId = MakeCanId(CanPriority::heartbeat, canSystemCategoryId,
            canHeartbeatMessageTypeId, fixture.config.nodeId);
        auto id = hal::Can::Id::Create29BitId(rawId);

        fixture.heartbeatsInjected += count;
        for (std::int32_t i = 0; i < count; ++i)
        {
            hal::Can::Message msg;
            msg.push_back(canProtocolVersion);
            fixture.serverCan.InjectFrame(id, msg);
        }
    }
}

WHEN(R"(the client sends {int} heartbeat messages to the server)", (std::int32_t count))
{
    InjectHeartbeats(context.Get<ApplicationFixture>(), count);
}

WHEN(R"({int} heartbeat messages reach the server)", (std::int32_t count))
{
    InjectHeartbeats(context.Get<ApplicationFixture>(), count);
}

WHEN(R"(a heartbeat message reaches the server)")
{
    InjectHeartbeats(context.Get<ApplicationFixture>(), 1);
}

THEN(R"(the server shall have processed {int} messages)", (std::int32_t expected))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_EQ(fixture.heartbeatsInjected - static_cast<std::int32_t>(fixture.server.Statistics().rateLimited), expected);
}
