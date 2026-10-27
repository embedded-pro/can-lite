#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "can-lite/transport/IsoTpTransportImpl.hpp"
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

        void RegisterDemoCategory();
        void RegisterDemoCategoryServerOnly();
        void RegisterFirmwareUpgrade();
        void ConnectIsoTp();
        uint32_t PduDataId() const;
        uint32_t PduFlowControlId() const;
        SequencedTestCategory& RegisterSequencedCategory(uint8_t id);
        SimpleTestCategory& RegisterSimpleCategory(uint8_t id);
        SequencedTestCategory* FindSequencedCategory(uint8_t id);

        VirtualCan serverCan;
        VirtualCan clientCan;
        Init init;
        services::CanProtocolServer::Config config;
        services::CanProtocolServer server;
        testing::StrictMock<ServerObserverMock> serverObserver;
        bool clientOnlineAsSeenByServer = false;
        int clientOnlineReports = 0;
        services::CanProtocolClient client;
        testing::StrictMock<CanProtocolClientObserverMock> clientObserver;
        int serversOnlineAsSeenByClient = 0;
        int commandAckTimeouts = 0;

        std::optional<DemoCategoryServer> demoServer;
        std::optional<testing::StrictMock<DemoServerObserverMock>> demoServerObserver;
        std::optional<DemoCategoryClient> demoClient;
        std::optional<testing::StrictMock<DemoClientObserverMock>> demoClientObserver;

        infra::Function<void()> capturedPingDone;
        infra::Function<void(int16_t)> capturedQueryValueResult;
        infra::Function<void(DemoError)> capturedFailResult;
        std::optional<DemoParameters> receivedParameters;
        std::optional<int16_t> receivedValue;
        std::optional<DemoError> receivedCategoryError;
        uint8_t receivedCategoryErrorCommand = 0;

        std::optional<services::FirmwareUpgradeCategoryServer> fwuServer;
        std::optional<testing::StrictMock<FirmwareUpgradeServerObserverMock>> fwuServerObserver;
        std::optional<services::FirmwareUpgradeCategoryClient> fwuClient;
        std::optional<testing::StrictMock<FirmwareUpgradeClientObserverMock>> fwuClientObserver;
        std::optional<uint32_t> fwuBeginSize;
        std::optional<services::FwuError> fwuBeginResponseStatus;
        bool fwuProgressQueried = false;
        bool fwuSessionTimedOut = false;

        std::optional<PduTestCategory> pduCategory;
        std::optional<services::IsoTpTransportImpl::WithStorage<64, 2>> serverIsoTp;
        std::optional<services::IsoTpTransportImpl::WithStorage<64, 2>> clientIsoTp;
        std::optional<services::iso_tp::AbortReason> serverIsoTpFailure;
        std::optional<services::iso_tp::AbortReason> clientIsoTpFailure;
        int clientTransfersCompleted = 0;

        infra::BoundedVector<SequencedTestCategory>::WithMaxSize<4> sequencedCategories;
        infra::BoundedVector<SimpleTestCategory>::WithMaxSize<4> simpleCategories;

        int heartbeatsInjected = 0;
    };
}
