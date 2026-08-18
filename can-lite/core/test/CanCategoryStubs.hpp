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
        uint8_t PeekSequence(uint16_t) override
        {
            return sequence;
        }

        void CommitSequence(uint16_t) override
        {
            ++sequence;
        }

    private:
        uint8_t sequence{};
    };

    // Held as a base class so its members are constructed before the CanCategory
    // subobject that receives references to them.
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
