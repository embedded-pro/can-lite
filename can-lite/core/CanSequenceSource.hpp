#pragma once

#include <cstdint>

namespace services
{
    class CanSequenceSource
    {
    public:
        virtual uint8_t ReserveSequence(uint16_t nodeId, uint8_t category, uint8_t messageType) = 0;
        virtual void CancelReservation(uint16_t nodeId, uint8_t sequence) = 0;
        virtual void ReturnSequence(uint16_t nodeId, uint8_t sequence) = 0;

    protected:
        CanSequenceSource() = default;
        CanSequenceSource(const CanSequenceSource&) = delete;
        CanSequenceSource& operator=(const CanSequenceSource&) = delete;
        ~CanSequenceSource() = default;
    };
}
