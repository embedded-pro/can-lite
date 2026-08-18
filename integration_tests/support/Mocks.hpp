#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "support/TestCategories.hpp"
#include "gmock/gmock.h"

namespace integration
{
    class DemoServerObserverMock
        : public DemoCategoryServerObserver
    {
    public:
        using DemoCategoryServerObserver::DemoCategoryServerObserver;

        MOCK_METHOD(void, OnPing, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetParameters, (const DemoParameters& parameters, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnQueryValue, (const infra::Function<void(int16_t)>& onResult), (override));
        MOCK_METHOD(void, OnFail, (const infra::Function<void(DemoError)>& onResult), (override));
    };

    class DemoClientObserverMock
        : public DemoCategoryClientObserver
    {
    public:
        using DemoCategoryClientObserver::DemoCategoryClientObserver;

        MOCK_METHOD(void, OnValueResponse, (int16_t value), (override));
        MOCK_METHOD(void, OnCategoryError, (uint8_t originatingCommandId, DemoError error), (override));
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
