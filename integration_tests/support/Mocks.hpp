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
        MOCK_METHOD(void, OnSelectControlMode, (services::FocMotorMode mode), (override));
        MOCK_METHOD(void, OnSetTorqueSetpoint, (int16_t value), (override));
        MOCK_METHOD(void, OnSetSpeedSetpoint, (int16_t value), (override));
        MOCK_METHOD(void, OnSetPositionSetpoint, (int16_t value), (override));
        MOCK_METHOD(void, OnClearFault, (), (override));
        MOCK_METHOD(void, OnEmergencyStop, (), (override));
        MOCK_METHOD(void, OnConfigureTelemetryRate, (uint8_t rateHz), (override));
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
        MOCK_METHOD(void, OnSelectControlModeResponse, (services::FocMotorMode activeMode, services::FocRejectReason reason), (override));
        MOCK_METHOD(void, OnCommandRejected, (uint8_t origCmdId, services::FocRejectReason reason), (override));
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

        MOCK_METHOD(void, OnBeginUpgrade, (uint32_t firmwareSize), (override));
        MOCK_METHOD(void, OnDataBlock, (uint16_t blockIndex, const hal::Can::Message& data), (override));
        MOCK_METHOD(void, OnVerify, (uint32_t expectedCrc32), (override));
        MOCK_METHOD(void, OnActivate, (), (override));
        MOCK_METHOD(void, OnAbort, (), (override));
        MOCK_METHOD(void, OnQueryProgress, (), (override));
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
