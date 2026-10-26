#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;
using namespace services;
using integration::ApplicationFixture;

GIVEN(R"(a CAN bus with a server at node {int} and rate limit {int})", (std::int32_t nodeId, std::int32_t rateLimit))
{
    context.Emplace<ApplicationFixture>(static_cast<uint16_t>(nodeId), static_cast<uint16_t>(rateLimit));
}

GIVEN(R"(a CAN bus client connected to the same bus)")
{
}
