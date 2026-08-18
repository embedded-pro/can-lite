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

        // Default: message types that don't opt in to PDU (ISO-TP) handling via
        // the 4-argument CanMessageHandler constructor simply don't support
        // multi-frame payloads; reject rather than crash.
        virtual bool HandlePdu(infra::ConstByteRange)
        {
            return false;
        }

    protected:
        CanMessageType() = default;
        CanMessageType(const CanMessageType&) = delete;
        CanMessageType& operator=(const CanMessageType&) = delete;
        ~CanMessageType() = default;
    };
}
