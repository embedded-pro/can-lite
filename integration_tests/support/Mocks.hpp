#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryClient.hpp"
#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "gmock/gmock.h"

namespace integration
{
    class FocMotorServerObserverMock
        : public services::FocMotorCategoryServerObserver
    {
    public:
        using services::FocMotorCategoryServerObserver::FocMotorCategoryServerObserver;

        MOCK_METHOD(void, OnQueryMotorType, (), (override));
        MOCK_METHOD(void, OnStart, (), (override));
        MOCK_METHOD(void, OnStop, (), (override));
        MOCK_METHOD(void, OnSetPidCurrent, (const services::FocPidGains& gains), (override));
        MOCK_METHOD(void, OnSetPidSpeed, (const services::FocPidGains& gains), (override));
        MOCK_METHOD(void, OnSetPidPosition, (const services::FocPidGains& gains), (override));
        MOCK_METHOD(void, OnIdentifyElectrical, (), (override));
        MOCK_METHOD(void, OnIdentifyMechanical, (), (override));
        MOCK_METHOD(void, OnRequestTelemetry, (), (override));
        MOCK_METHOD(void, OnSetEncoderResolution, (uint16_t resolution), (override));
    };

    class FocMotorClientObserverMock
        : public services::FocMotorCategoryClientObserver
    {
    public:
        using services::FocMotorCategoryClientObserver::FocMotorCategoryClientObserver;

        MOCK_METHOD(void, OnMotorTypeResponse, (services::FocMotorMode mode), (override));
        MOCK_METHOD(void, OnElectricalParamsResponse, (const services::FocElectricalParams& params), (override));
        MOCK_METHOD(void, OnMechanicalParamsResponse, (const services::FocMechanicalParams& params), (override));
        MOCK_METHOD(void, OnTelemetryElectricalResponse, (const services::FocTelemetryElectrical& telemetry), (override));
        MOCK_METHOD(void, OnTelemetryStatusResponse, (const services::FocTelemetryStatus& status), (override));
    };

    class ServerObserverMock
        : public services::CanProtocolServerObserver
    {
    public:
        using services::CanProtocolServerObserver::CanProtocolServerObserver;

        MOCK_METHOD(void, Online, (), (override));
        MOCK_METHOD(void, Offline, (), (override));
    };
}
