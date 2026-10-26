#include "can-lite/drivers/implementation/PlatformCanAdapter.hpp"
#include "can-lite/drivers/test/CanBusAdapterMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::StrictMock;

    MATCHER_P(IsText, expected, "")
    {
        return arg == infra::BoundedConstString(expected);
    }

    class TestPlatformCanAdapter
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        PlatformCanAdapter platform;
        StrictMock<CanBusAdapterObserverMock> observer{ platform.Adapter() };
    };
}

TEST_F(TestPlatformCanAdapter, starts_disconnected)
{
    EXPECT_FALSE(platform.Adapter().IsConnected());
    EXPECT_EQ(platform.Adapter().FileDescriptor(), -1);
}

TEST_F(TestPlatformCanAdapter, routes_a_prefixed_name_to_the_slcan_adapter)
{
    EXPECT_CALL(observer, OnError(IsText("Serial port not found")));

    EXPECT_FALSE(platform.Adapter().Connect("slcan:/dev/can-lite-no-such-port", 500000));
    EXPECT_FALSE(platform.Adapter().IsConnected());
}

TEST_F(TestPlatformCanAdapter, explains_an_unknown_prefix)
{
    EXPECT_CALL(observer, OnError(_));

    EXPECT_FALSE(platform.Adapter().Connect("nonsense:0", 500000));
}
