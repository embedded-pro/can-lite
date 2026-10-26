#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gtest/gtest.h"

HOOK_BEFORE_SCENARIO()
{
}

HOOK_AFTER_STEP()
{
    if (context.Contains<integration::ApplicationFixture>())
        context.Get<integration::ApplicationFixture>().ExecuteAllActions();
}

HOOK_AFTER_SCENARIO()
{
    if (context.Contains<integration::ApplicationFixture>())
        EXPECT_EQ(context.Get<integration::ApplicationFixture>().commandAckTimeouts, 0);
}
