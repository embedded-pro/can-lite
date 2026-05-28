#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/categories/foc_motor/FocMotorCategoryClient.hpp"
#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "gmock/gmock.h"

namespace integration
{
    class FocMotorServerObserverMock
        : public services::FocMotorCategoryServerObserver
    {
    public:
        using services::FocMotorCategoryServerObserver::FocMotorCategoryServerObserver;

        MOCK_METHOD(void, OnQueryMotorType, (const infra::Function<void(services::FocMotorMode)>& onResult), (override));
        MOCK_METHOD(void, OnStart, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnStop, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPidCurrent, (const services::FocPidGains& gains, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPidSpeed, (const services::FocPidGains& gains, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPidPosition, (const services::FocPidGains& gains, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnIdentifyElectrical, (const infra::Function<void(services::FocElectricalParams)>& onResult), (override));
        MOCK_METHOD(void, OnIdentifyMechanical, (const infra::Function<void(services::FocMechanicalParams)>& onResult), (override));
        MOCK_METHOD(void, OnRequestTelemetry, (const infra::Function<void(services::FocTelemetryElectrical, services::FocTelemetryStatus)>& onResult), (override));
        MOCK_METHOD(void, OnSetEncoderResolution, (uint16_t resolution, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSelectControlMode, (services::FocMotorMode mode, const infra::Function<void(services::FocMotorMode)>& onActivated), (override));
        MOCK_METHOD(void, OnSetTorqueSetpoint, (int16_t value, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetSpeedSetpoint, (int16_t value, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPositionSetpoint, (int16_t value, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnClearFault, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnEmergencyStop, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnConfigureTelemetryRate, (uint8_t rateHz, const infra::Function<void()>& onDone), (override));
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
        MOCK_METHOD(void, OnSelectControlModeResponse, (services::FocMotorMode activeMode), (override));
    };

    class ServerObserverMock
        : public services::CanProtocolServerObserver
    {
    public:
        using services::CanProtocolServerObserver::CanProtocolServerObserver;

        MOCK_METHOD(void, Online, (), (override));
        MOCK_METHOD(void, Offline, (), (override));
    };

    class CanProtocolClientObserverMock
        : public services::CanProtocolClientObserver
    {
    public:
        using services::CanProtocolClientObserver::CanProtocolClientObserver;

        MOCK_METHOD(void, OnServerOnline, (uint16_t nodeId), (override));
        MOCK_METHOD(void, OnServerOffline, (uint16_t nodeId), (override));
    };

    class FirmwareUpgradeServerObserverMock
        : public services::FirmwareUpgradeCategoryServerObserver
    {
    public:
        using services::FirmwareUpgradeCategoryServerObserver::FirmwareUpgradeCategoryServerObserver;

        MOCK_METHOD(void, OnBeginUpgrade, (uint32_t firmwareSize, const infra::Function<void(services::FwuError, uint16_t)>& onResult), (override));
        MOCK_METHOD(void, OnDataBlock, (uint16_t blockIndex, const hal::Can::Message& data, const infra::Function<void(services::FwuError)>& onResult), (override));
        MOCK_METHOD(void, OnVerify, (uint32_t expectedCrc32, const infra::Function<void(services::FwuError)>& onResult), (override));
        MOCK_METHOD(void, OnActivate, (const infra::Function<void(services::FwuError)>& onResult), (override));
        MOCK_METHOD(void, OnAbort, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnQueryProgress, (const infra::Function<void(services::FwuState, uint16_t, uint16_t)>& onResult), (override));
        MOCK_METHOD(void, OnSessionTimeout, (), (override));
    };

    class FirmwareUpgradeClientObserverMock
        : public services::FirmwareUpgradeCategoryClientObserver
    {
    public:
        using services::FirmwareUpgradeCategoryClientObserver::FirmwareUpgradeCategoryClientObserver;

        MOCK_METHOD(void, OnBeginResponse, (services::FwuError status, uint16_t pageSize), (override));
        MOCK_METHOD(void, OnDataBlockAck, (services::FwuError status, uint16_t blockIndex), (override));
        MOCK_METHOD(void, OnVerifyResponse, (services::FwuError status), (override));
        MOCK_METHOD(void, OnActivateResponse, (services::FwuError status), (override));
        MOCK_METHOD(void, OnProgressResponse, (services::FwuState state, uint16_t blocksReceived, uint16_t totalBlocks), (override));
    };
}
