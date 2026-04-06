#pragma once

#include "can-lite/transport/iso-tp/IsoTpTypes.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace services
{
    class IsoTpTransport
    {
    public:
        virtual bool RegisterReceiveChannel(uint32_t dataId, uint32_t fcId) = 0;

        virtual bool SendPdu(uint32_t dataId, uint32_t fcId,
            infra::ConstByteRange pdu,
            const infra::Function<void()>& onDone) = 0;

        virtual bool ProcessFrame(uint32_t canId, const hal::Can::Message& frame) = 0;

        virtual void SetOnPduReceived(
            infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> callback) = 0;

        virtual void SetOnAbort(
            infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> callback) = 0;

    protected:
        ~IsoTpTransport() = default;
    };
}
