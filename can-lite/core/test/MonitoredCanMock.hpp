#pragma once

#include "can-lite/core/MonitoredCan.hpp"
#include "gmock/gmock.h"

namespace services
{
    class MonitoredCanMock
        : public MonitoredCan
    {
    public:
        MOCK_METHOD(void, SendData, (Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion), (override));
        MOCK_METHOD(void, ReceiveData, (const infra::Function<void(Id id, const Message& data)>& receivedAction), (override));
        MOCK_METHOD(void, SetOnError, (const infra::Function<void(CanBusError)>& handler), (override));
    };
}
