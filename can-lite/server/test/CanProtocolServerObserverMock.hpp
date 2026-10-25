#pragma once

#include "can-lite/server/CanProtocolServer.hpp"
#include "gmock/gmock.h"

namespace services
{
    class CanProtocolServerObserverMock
        : public CanProtocolServerObserver
    {
    public:
        using CanProtocolServerObserver::CanProtocolServerObserver;

        MOCK_METHOD(void, Online, (), (override));
        MOCK_METHOD(void, Offline, (), (override));
    };
}
