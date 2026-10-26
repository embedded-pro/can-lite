#pragma once

#include "can-lite/client/CanProtocolClient.hpp"
#include "gmock/gmock.h"

namespace services
{
    class CanProtocolClientObserverMock
        : public CanProtocolClientObserver
    {
    public:
        using CanProtocolClientObserver::CanProtocolClientObserver;

        MOCK_METHOD(void, OnServerOnline, (uint16_t nodeId), (override));
        MOCK_METHOD(void, OnServerOffline, (uint16_t nodeId), (override));
        MOCK_METHOD(void, OnCommandAckTimeout, (uint16_t nodeId, uint8_t category, uint8_t messageType), (override));
    };
}
