#pragma once

#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "gmock/gmock.h"

namespace integration
{
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
}
