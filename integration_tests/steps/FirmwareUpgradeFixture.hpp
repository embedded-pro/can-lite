#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "support/ApplicationFixture.hpp"
#include "gmock/gmock.h"

namespace integration
{
    class FirmwareUpgradeServerObserverMock
        : public services::FirmwareUpgradeCategoryServerObserver
    {
    public:
        using services::FirmwareUpgradeCategoryServerObserver::FirmwareUpgradeCategoryServerObserver;

        MOCK_METHOD(void, OnBeginUpgrade, (uint32_t firmwareSize, const infra::Function<void(services::FwuError, uint16_t)>& onResult), (override));
        MOCK_METHOD(void, OnDataBlock, (uint16_t blockIndex, infra::ConstByteRange data, const infra::Function<void(services::FwuError)>& onResult), (override));
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

    // The firmware upgrade category is a domain category, so its composition
    // lives with the steps that need it rather than in the shared fixture.
    struct FirmwareUpgradeFixture
    {
        explicit FirmwareUpgradeFixture(ApplicationFixture& application);
        ~FirmwareUpgradeFixture();

        ApplicationFixture& application;
        services::FirmwareUpgradeCategoryServer::Config config{};
        services::FirmwareUpgradeCategoryServer server{ config };
        testing::StrictMock<FirmwareUpgradeServerObserverMock> serverObserver{ server };
        services::FirmwareUpgradeCategoryClient client;
        testing::StrictMock<FirmwareUpgradeClientObserverMock> clientObserver{ client };
    };
}
