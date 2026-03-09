#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;
using namespace services;
using integration::ApplicationFixture;

GIVEN(R"(the FOC motor category is registered on both client and server)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.RegisterFocMotor();
}

WHEN(R"(the client sends a query motor type command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.motorServerObserver, OnQueryMotorType());
    fixture.motorClient->SendQueryMotorType();
}

THEN(R"(the server observer shall receive an OnQueryMotorType event)")
{
    SUCCEED();
}

WHEN(R"(the client sends a start command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.motorServerObserver, OnStart());
    fixture.motorClient->SendStart();
}

THEN(R"(the server observer shall receive an OnStart event)")
{
    SUCCEED();
}

WHEN(R"(the client sends a stop command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.motorServerObserver, OnStop());
    fixture.motorClient->SendStop();
}

THEN(R"(the server observer shall receive an OnStop event)")
{
    SUCCEED();
}

WHEN(R"(the client sends a set PID current command with kp {int}, ki {int}, kd {int})", (std::int32_t kp, std::int32_t ki, std::int32_t kd))
{
    auto& fixture = context.Get<ApplicationFixture>();

    EXPECT_CALL(*fixture.motorServerObserver, OnSetPidCurrent(_)).WillOnce([kp, ki, kd](const FocPidGains& gains)
        {
            EXPECT_EQ(gains.kp, static_cast<int16_t>(kp));
            EXPECT_EQ(gains.ki, static_cast<int16_t>(ki));
            EXPECT_EQ(gains.kd, static_cast<int16_t>(kd));
        });

    FocPidGains gains{ static_cast<int16_t>(kp), static_cast<int16_t>(ki), static_cast<int16_t>(kd) };
    fixture.motorClient->SendSetPidCurrent(gains);
}

THEN(R"(the server observer shall receive PID current gains kp {int}, ki {int}, kd {int})", (std::int32_t, std::int32_t, std::int32_t))
{
    SUCCEED();
}

WHEN(R"(the client sends a set PID speed command with kp {int}, ki {int}, kd {int})", (std::int32_t kp, std::int32_t ki, std::int32_t kd))
{
    auto& fixture = context.Get<ApplicationFixture>();

    EXPECT_CALL(*fixture.motorServerObserver, OnSetPidSpeed(_)).WillOnce([kp, ki, kd](const FocPidGains& gains)
        {
            EXPECT_EQ(gains.kp, static_cast<int16_t>(kp));
            EXPECT_EQ(gains.ki, static_cast<int16_t>(ki));
            EXPECT_EQ(gains.kd, static_cast<int16_t>(kd));
        });

    FocPidGains gains{ static_cast<int16_t>(kp), static_cast<int16_t>(ki), static_cast<int16_t>(kd) };
    fixture.motorClient->SendSetPidSpeed(gains);
}

THEN(R"(the server observer shall receive PID speed gains kp {int}, ki {int}, kd {int})", (std::int32_t, std::int32_t, std::int32_t))
{
    SUCCEED();
}

WHEN(R"(the client sends a set PID position command with kp {int}, ki {int}, kd {int})", (std::int32_t kp, std::int32_t ki, std::int32_t kd))
{
    auto& fixture = context.Get<ApplicationFixture>();

    EXPECT_CALL(*fixture.motorServerObserver, OnSetPidPosition(_)).WillOnce([kp, ki, kd](const FocPidGains& gains)
        {
            EXPECT_EQ(gains.kp, static_cast<int16_t>(kp));
            EXPECT_EQ(gains.ki, static_cast<int16_t>(ki));
            EXPECT_EQ(gains.kd, static_cast<int16_t>(kd));
        });

    FocPidGains gains{ static_cast<int16_t>(kp), static_cast<int16_t>(ki), static_cast<int16_t>(kd) };
    fixture.motorClient->SendSetPidPosition(gains);
}

THEN(R"(the server observer shall receive PID position gains kp {int}, ki {int}, kd {int})", (std::int32_t, std::int32_t, std::int32_t))
{
    SUCCEED();
}

WHEN(R"(the client sends an identify electrical command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.motorServerObserver, OnIdentifyElectrical());
    fixture.motorClient->SendIdentifyElectrical();
}

THEN(R"(the server observer shall receive an OnIdentifyElectrical event)")
{
    SUCCEED();
}

WHEN(R"(the client sends an identify mechanical command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.motorServerObserver, OnIdentifyMechanical());
    fixture.motorClient->SendIdentifyMechanical();
}

THEN(R"(the server observer shall receive an OnIdentifyMechanical event)")
{
    SUCCEED();
}

WHEN(R"(the client sends a request telemetry command)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.motorServerObserver, OnRequestTelemetry());
    fixture.motorClient->SendRequestTelemetry();
}

THEN(R"(the server observer shall receive an OnRequestTelemetry event)")
{
    SUCCEED();
}

WHEN(R"(the client sends a set encoder resolution command with resolution {int})", (std::int32_t resolution))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_CALL(*fixture.motorServerObserver, OnSetEncoderResolution(static_cast<uint16_t>(resolution)));
    fixture.motorClient->SendSetEncoderResolution(static_cast<uint16_t>(resolution));
}

THEN(R"(the server observer shall receive encoder resolution {int})", (std::int32_t))
{
    SUCCEED();
}

static FocMotorMode ParseMotorMode(const std::string& mode)
{
    if (mode == "torque")
        return FocMotorMode::torque;
    if (mode == "speed")
        return FocMotorMode::speed;
    if (mode == "position")
        return FocMotorMode::position;
    return FocMotorMode::torque;
}

static FocMotorState ParseMotorState(const std::string& state)
{
    if (state == "idle")
        return FocMotorState::idle;
    if (state == "running")
        return FocMotorState::running;
    if (state == "fault")
        return FocMotorState::fault;
    if (state == "calibrating")
        return FocMotorState::calibrating;
    return FocMotorState::idle;
}

static FocFaultCode ParseFaultCode(const std::string& fault)
{
    if (fault == "none")
        return FocFaultCode::none;
    if (fault == "overCurrent")
        return FocFaultCode::overCurrent;
    if (fault == "overVoltage")
        return FocFaultCode::overVoltage;
    if (fault == "underVoltage")
        return FocFaultCode::underVoltage;
    if (fault == "overTemperature")
        return FocFaultCode::overTemperature;
    if (fault == "sensorFault")
        return FocFaultCode::sensorFault;
    return FocFaultCode::none;
}

WHEN(R"(the server sends a motor type response with mode {string})", (const std::string& mode))
{
    auto& fixture = context.Get<ApplicationFixture>();
    auto expectedMode = ParseMotorMode(mode);

    EXPECT_CALL(*fixture.motorClientObserver, OnMotorTypeResponse(expectedMode));
    fixture.motorServer->SendMotorTypeResponse(expectedMode);
}

THEN(R"(the client observer shall receive a motor type response with mode {string})", (const std::string&))
{
    SUCCEED();
}

WHEN(R"(the server sends electrical params with resistance {int} and inductance {int})", (std::int32_t resistance, std::int32_t inductance))
{
    auto& fixture = context.Get<ApplicationFixture>();
    FocElectricalParams params{ static_cast<int16_t>(resistance), static_cast<int16_t>(inductance) };

    EXPECT_CALL(*fixture.motorClientObserver, OnElectricalParamsResponse(_)).WillOnce([resistance, inductance](const FocElectricalParams& p)
        {
            EXPECT_EQ(p.resistance, static_cast<int16_t>(resistance));
            EXPECT_EQ(p.inductance, static_cast<int16_t>(inductance));
        });
    fixture.motorServer->SendElectricalParamsResponse(params);
}

THEN(R"(the client observer shall receive electrical params with resistance {int} and inductance {int})", (std::int32_t, std::int32_t))
{
    SUCCEED();
}

WHEN(R"(the server sends mechanical params with inertia {int} and friction {int})", (std::int32_t inertia, std::int32_t friction))
{
    auto& fixture = context.Get<ApplicationFixture>();
    FocMechanicalParams params{ static_cast<int16_t>(inertia), static_cast<int16_t>(friction) };

    EXPECT_CALL(*fixture.motorClientObserver, OnMechanicalParamsResponse(_)).WillOnce([inertia, friction](const FocMechanicalParams& p)
        {
            EXPECT_EQ(p.inertia, static_cast<int16_t>(inertia));
            EXPECT_EQ(p.friction, static_cast<int16_t>(friction));
        });
    fixture.motorServer->SendMechanicalParamsResponse(params);
}

THEN(R"(the client observer shall receive mechanical params with inertia {int} and friction {int})", (std::int32_t, std::int32_t))
{
    SUCCEED();
}

WHEN(R"(the server sends electrical telemetry with voltage {int}, maxCurrent {int}, iq {int}, id {int})",
    (std::int32_t voltage, std::int32_t maxCurrent, std::int32_t iq, std::int32_t id))
{
    auto& fixture = context.Get<ApplicationFixture>();
    FocTelemetryElectrical telemetry{
        static_cast<int16_t>(voltage),
        static_cast<int16_t>(maxCurrent),
        static_cast<int16_t>(iq),
        static_cast<int16_t>(id)
    };

    EXPECT_CALL(*fixture.motorClientObserver, OnTelemetryElectricalResponse(_)).WillOnce([voltage, maxCurrent, iq, id](const FocTelemetryElectrical& t)
        {
            EXPECT_EQ(t.voltage, static_cast<int16_t>(voltage));
            EXPECT_EQ(t.maxCurrent, static_cast<int16_t>(maxCurrent));
            EXPECT_EQ(t.iq, static_cast<int16_t>(iq));
            EXPECT_EQ(t.id, static_cast<int16_t>(id));
        });
    fixture.motorServer->SendTelemetryElectricalResponse(telemetry);
}

THEN(R"(the client observer shall receive electrical telemetry with voltage {int}, maxCurrent {int}, iq {int}, id {int})",
    (std::int32_t, std::int32_t, std::int32_t, std::int32_t))
{
    SUCCEED();
}

WHEN(R"(the server sends telemetry status with state {string}, fault {string}, speed {int}, position {int})",
    (const std::string& state, const std::string& fault, std::int32_t speed, std::int32_t position))
{
    auto& fixture = context.Get<ApplicationFixture>();
    FocTelemetryStatus status{
        ParseMotorState(state),
        ParseFaultCode(fault),
        static_cast<int16_t>(speed),
        static_cast<int16_t>(position)
    };

    EXPECT_CALL(*fixture.motorClientObserver, OnTelemetryStatusResponse(_)).WillOnce([state, fault, speed, position](const FocTelemetryStatus& s)
        {
            EXPECT_EQ(s.state, ParseMotorState(state));
            EXPECT_EQ(s.fault, ParseFaultCode(fault));
            EXPECT_EQ(s.speed, static_cast<int16_t>(speed));
            EXPECT_EQ(s.position, static_cast<int16_t>(position));
        });
    fixture.motorServer->SendTelemetryStatusResponse(status);
}

THEN(R"(the client observer shall receive telemetry status with state {string}, fault {string}, speed {int}, position {int})",
    (const std::string&, const std::string&, std::int32_t, std::int32_t))
{
    SUCCEED();
}
