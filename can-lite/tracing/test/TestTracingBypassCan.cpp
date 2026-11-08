#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/tracing/TracingBypassCan.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace testing;

    constexpr uint8_t streamCategory{ 0x02 };
    constexpr uint8_t streamMessageType{ 0x90 };

    hal::Can::Id Frame(uint8_t category, uint8_t messageType)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::telemetry, category, messageType, 1));
    }

    bool IsStream(hal::Can::Id id)
    {
        return id.Is29BitId() && services::ExtractCanCategory(id.Get29BitId()) == streamCategory &&
               services::ExtractCanMessageType(id.Get29BitId()) == streamMessageType;
    }

    class TracingBypassCanTest
        : public Test
    {
    public:
        StrictMock<hal::CanMock> traced;
        StrictMock<hal::CanMock> untraced;
        services::TracingBypassCan bus{ traced, untraced, [](hal::Can::Id id)
            {
                return IsStream(id);
            } };
        hal::Can::Message payload;
    };
}

TEST_F(TracingBypassCanTest, a_frame_the_predicate_selects_bypasses_the_tracing_bus)
{
    const auto id = Frame(streamCategory, streamMessageType);
    EXPECT_CALL(untraced, SendData(id, _, _));

    bus.SendData(id, payload, [](bool) {});
}

TEST_F(TracingBypassCanTest, every_other_frame_goes_through_the_tracing_bus)
{
    const auto other = Frame(streamCategory, 0x91);
    const auto otherCategory = Frame(services::canSystemCategoryId, streamMessageType);
    const auto standard = hal::Can::Id::Create11BitId(0x123);
    EXPECT_CALL(traced, SendData(other, _, _));
    EXPECT_CALL(traced, SendData(otherCategory, _, _));
    EXPECT_CALL(traced, SendData(standard, _, _));

    bus.SendData(other, payload, [](bool) {});
    bus.SendData(otherCategory, payload, [](bool) {});
    bus.SendData(standard, payload, [](bool) {});
}

TEST_F(TracingBypassCanTest, received_frames_come_through_the_tracing_bus)
{
    EXPECT_CALL(traced, ReceiveData(_));

    bus.ReceiveData([](hal::Can::Id, const hal::Can::Message&) {});
}
