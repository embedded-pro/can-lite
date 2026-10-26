#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "gmock/gmock.h"

namespace services
{
    class CanSequenceSourceStub
        : public CanSequenceSource
    {
    public:
        uint8_t ReserveSequence(uint16_t, uint8_t, uint8_t) override
        {
            return sequence++;
        }

        void CancelReservation(uint16_t, uint8_t reserved) override
        {
            sequence = reserved;
        }

        void ReturnSequence(uint16_t, uint8_t returned) override
        {
            if (sequence == static_cast<uint8_t>(returned + 1))
                sequence = returned;
        }

    private:
        uint8_t sequence{};
    };

    class CanCategoryContextStub
    {
    protected:
        testing::StrictMock<hal::CanMock> stubCan;
        CanFrameTransport stubTransport{ stubCan, 0 };
        CanSequenceSourceStub stubSequenceSource;
    };

    class CanCategoryServerStub
        : private CanCategoryContextStub
        , public CanCategoryServer
    {
    protected:
        CanCategoryServerStub()
            : CanCategoryServer(stubTransport)
        {}
    };

    class CanCategoryClientStub
        : private CanCategoryContextStub
        , public CanCategoryClient
    {
    protected:
        CanCategoryClientStub()
            : CanCategoryClient(stubTransport, stubSequenceSource)
        {}
    };
}
