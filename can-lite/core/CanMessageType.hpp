#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/IntrusiveList.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <cstdint>

namespace services
{
    class CanMessageType
        : public infra::IntrusiveList<CanMessageType>::NodeType
    {
    public:
        virtual uint8_t Id() const = 0;
        virtual void Handle(const hal::Can::Message& data) = 0;

        virtual bool HandlePdu(infra::ConstByteRange data)
        {
            really_assert(false);
            return false;
        }

    protected:
        CanMessageType() = default;
        CanMessageType(const CanMessageType&) = delete;
        CanMessageType& operator=(const CanMessageType&) = delete;
        ~CanMessageType() = default;
    };
}
