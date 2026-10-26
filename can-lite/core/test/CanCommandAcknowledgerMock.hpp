#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "gmock/gmock.h"

namespace services
{
    class CanCommandAcknowledgerMock
        : public CanCommandAcknowledger
    {
    public:
        MOCK_METHOD(void, SendCommandAck, (uint8_t category, uint8_t messageType, CanAckStatus status), (override));
    };
}
