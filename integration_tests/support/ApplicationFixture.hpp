#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryClient.hpp"
#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "support/Mocks.hpp"
#include "support/TestCategories.hpp"
#include "support/VirtualCan.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace integration
{
    struct ApplicationFixture : infra::ClockFixture
    {
        struct Init
        {
            Init(VirtualCan& server, VirtualCan& client);
        };

        ApplicationFixture(uint16_t nodeId, uint16_t rateLimit);

        void RegisterFocMotor();
        void RegisterFocMotorServerOnly();
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

        std::optional<services::CanFrameTransport> serverTransport;
        std::optional<services::FocMotorCategoryServer> motorServer;
        std::optional<testing::StrictMock<FocMotorServerObserverMock>> motorServerObserver;
        std::optional<services::CanFrameTransport> clientTransport;
        std::optional<services::FocMotorCategoryClient> motorClient;
        std::optional<testing::StrictMock<FocMotorClientObserverMock>> motorClientObserver;

        std::vector<std::unique_ptr<SequencedTestCategory>> sequencedCategories;
        std::vector<std::unique_ptr<SimpleTestCategory>> simpleCategories;

        int processedCount = 0;
    };
}
