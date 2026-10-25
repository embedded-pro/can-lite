#pragma once

#include "can-lite/core/CanSequenceSource.hpp"
#include "gmock/gmock.h"

namespace services
{
    class CanSequenceSourceMock
        : public CanSequenceSource
    {
    public:
        MOCK_METHOD(uint8_t, ReserveSequence, (uint16_t nodeId, uint8_t category, uint8_t messageType), (override));
        MOCK_METHOD(void, CancelReservation, (uint16_t nodeId, uint8_t sequence), (override));
        MOCK_METHOD(void, ReturnSequence, (uint16_t nodeId, uint8_t sequence), (override));
    };
}
