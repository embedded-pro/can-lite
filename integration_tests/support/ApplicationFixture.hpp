#pragma once

#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "can-lite/testing/EchoCategoryClient.hpp"
#include "can-lite/testing/EchoCategoryServer.hpp"
#include "can-lite/testing/VirtualCan.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/BoundedVector.hpp"
#include "support/Mocks.hpp"

namespace integration
{
    // The fixture composes nothing but the protocol itself and the generic echo
    // category, so a scenario that needs a domain category brings its own.
    struct ApplicationFixture : infra::ClockFixture
    {
        struct Init
        {
            Init(services::VirtualCan& server, services::VirtualCan& client);
        };

        ApplicationFixture(uint16_t nodeId, uint16_t rateLimit);
        ~ApplicationFixture();

        services::EchoCategoryServer& RegisterEchoCategory(uint8_t id, bool requiresSequenceValidation);
        services::EchoCategoryServer* FindEchoCategory(uint8_t id);

        services::VirtualCan serverCan;
        services::VirtualCan clientCan;
        Init init;
        services::CanProtocolServer::Config config;
        services::CanProtocolServer server;
        testing::StrictMock<ServerObserverMock> serverObserver;
        services::CanProtocolClient client;

        infra::BoundedVector<services::EchoCategoryServer>::WithMaxSize<4> echoCategories;

        int processedCount = 0;
    };
}
