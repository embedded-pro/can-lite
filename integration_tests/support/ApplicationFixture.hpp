#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/categories/foc_motor/FocMotorCategoryClient.hpp"
#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/BoundedVector.hpp"
#include "support/Mocks.hpp"
#include "support/TestCategories.hpp"
#include "support/VirtualCan.hpp"
#include <optional>

namespace integration
{
    struct ApplicationFixture : infra::ClockFixture
    {
        struct Init
        {
            Init(VirtualCan& server, VirtualCan& client);
        };

        ApplicationFixture(uint16_t nodeId, uint16_t rateLimit);
        ~ApplicationFixture();

        void RegisterFocMotor();
        void RegisterFocMotorServerOnly();
        void RegisterFirmwareUpgrade();
        SequencedTestCategory& RegisterSequencedCategory(uint8_t id);
        SimpleTestCategory& RegisterSimpleCategory(uint8_t id);
        SequencedTestCategory* FindSequencedCategory(uint8_t id);

        VirtualCan serverCan;
        VirtualCan clientCan;
        Init init;
        services::CanProtocolServer::Config config;
        services::CanProtocolServer server;
        testing::StrictMock<ServerObserverMock> serverObserver;
        services::CanProtocolClient client;

        std::optional<services::FocMotorCategoryServer> motorServer;
        std::optional<testing::StrictMock<FocMotorServerObserverMock>> motorServerObserver;
        std::optional<services::CanFrameTransport> clientTransport;
        std::optional<services::FocMotorCategoryClient> motorClient;
        std::optional<testing::StrictMock<FocMotorClientObserverMock>> motorClientObserver;

        infra::Function<void(services::FocMotorMode)> capturedQueryMotorTypeResult;
        infra::Function<void(services::FocElectricalParams)> capturedIdentifyElectricalResult;
        infra::Function<void(services::FocMechanicalParams)> capturedIdentifyMechanicalResult;
        infra::Function<void(services::FocTelemetryElectrical, services::FocTelemetryStatus)> capturedRequestTelemetryResult;

        std::optional<services::FirmwareUpgradeCategoryServer> fwuServer;
        std::optional<testing::StrictMock<FirmwareUpgradeServerObserverMock>> fwuServerObserver;
        std::optional<services::CanFrameTransport> fwuClientTransport;
        std::optional<services::FirmwareUpgradeCategoryClient> fwuClient;
        std::optional<testing::StrictMock<FirmwareUpgradeClientObserverMock>> fwuClientObserver;

        infra::BoundedVector<SequencedTestCategory>::WithMaxSize<4> sequencedCategories;
        infra::BoundedVector<SimpleTestCategory>::WithMaxSize<4> simpleCategories;

        int processedCount = 0;
    };
}
