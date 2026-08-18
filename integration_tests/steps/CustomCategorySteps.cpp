#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;
using namespace services;
using integration::ApplicationFixture;
using integration::DemoError;
using integration::DemoParameters;

GIVEN(R"(the demo category is registered on both client and server)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.RegisterDemoCategory();
}

WHEN(R"(the client sends a ping command)")
{
    auto& fixture = context.Get<ApplicationFixture>();

    EXPECT_CALL(*fixture.demoServerObserver, OnPing(_))
        .WillOnce(Invoke([&fixture](const infra::Function<void()>& onDone)
            {
                fixture.capturedPingDone = onDone;
            }));

    fixture.demoClient->SendPing(fixture.config.nodeId);
}

THEN(R"(the server observer shall receive an OnPing event)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_TRUE(static_cast<bool>(fixture.capturedPingDone));
}

WHEN(R"(the server completes the ping command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.capturedPingDone();
}

WHEN(R"(the client sends a set parameters command with values {int}, {int}, {int})", (std::int32_t first, std::int32_t second, std::int32_t third))
{
    auto& fixture = context.Get<ApplicationFixture>();

    EXPECT_CALL(*fixture.demoServerObserver, OnSetParameters(_, _))
        .WillOnce(Invoke([&fixture](const DemoParameters& parameters, const infra::Function<void()>& onDone)
            {
                fixture.receivedParameters = parameters;
                onDone();
            }));

    DemoParameters parameters{ static_cast<int16_t>(first), static_cast<int16_t>(second), static_cast<int16_t>(third) };
    fixture.demoClient->SendSetParameters(fixture.config.nodeId, parameters);
}

THEN(R"(the server observer shall receive parameters {int}, {int}, {int})", (std::int32_t first, std::int32_t second, std::int32_t third))
{
    auto& fixture = context.Get<ApplicationFixture>();

    ASSERT_TRUE(fixture.receivedParameters.has_value());
    EXPECT_EQ(fixture.receivedParameters->first, static_cast<int16_t>(first));
    EXPECT_EQ(fixture.receivedParameters->second, static_cast<int16_t>(second));
    EXPECT_EQ(fixture.receivedParameters->third, static_cast<int16_t>(third));
}

WHEN(R"(the client sends a query value command)")
{
    auto& fixture = context.Get<ApplicationFixture>();

    EXPECT_CALL(*fixture.demoServerObserver, OnQueryValue(_))
        .WillOnce(Invoke([&fixture](const infra::Function<void(int16_t)>& onResult)
            {
                fixture.capturedQueryValueResult = onResult;
            }));

    fixture.demoClient->SendQueryValue(fixture.config.nodeId);
}

THEN(R"(the server observer shall receive an OnQueryValue event)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_TRUE(static_cast<bool>(fixture.capturedQueryValueResult));
}

WHEN(R"(the server responds with value {int})", (std::int32_t value))
{
    auto& fixture = context.Get<ApplicationFixture>();

    EXPECT_CALL(*fixture.demoClientObserver, OnValueResponse(static_cast<int16_t>(value)));
    fixture.capturedQueryValueResult(static_cast<int16_t>(value));
}

THEN(R"(the client observer shall receive a value response of {int})", (std::int32_t))
{
    SUCCEED();
}

WHEN(R"(the client sends a failing command)")
{
    auto& fixture = context.Get<ApplicationFixture>();

    EXPECT_CALL(*fixture.demoServerObserver, OnFail(_))
        .WillOnce(Invoke([&fixture](const infra::Function<void(DemoError)>& onResult)
            {
                fixture.capturedFailResult = onResult;
            }));

    fixture.demoClient->SendFail(fixture.config.nodeId);
}

THEN(R"(the server observer shall receive an OnFail event)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_TRUE(static_cast<bool>(fixture.capturedFailResult));
}

WHEN(R"(the server reports category error {string})", (const std::string& errorStr))
{
    auto& fixture = context.Get<ApplicationFixture>();

    auto error = errorStr == "busy" ? DemoError::busy : DemoError::notSupported;

    EXPECT_CALL(*fixture.demoClientObserver, OnCategoryError(integration::demoFailId, error));
    fixture.capturedFailResult(error);
}

THEN(R"(the client observer shall receive a category error {string} for command {int})", (const std::string&, std::int32_t))
{
    SUCCEED();
}
