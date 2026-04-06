#pragma once

#include "can-lite/transport/iso-tp/IsoTpTypes.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace services::iso_tp
{
    class IsoTpChannel
    {
    public:
        using RawSendFunc = infra::Function<bool(uint32_t canId, const hal::Can::Message& frame,
            const infra::Function<void()>& onDone)>;
        using PduReadyFunc = infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)>;
        using AbortFunc = infra::Function<void(uint32_t dataId, AbortReason reason)>;

        virtual void Configure(uint32_t dataId, uint32_t fcId,
            RawSendFunc rawSend, PduReadyFunc onPduReady, AbortFunc onAbort) = 0;

        virtual void Release() = 0;

        virtual bool IsOccupied() const = 0;
        virtual uint32_t DataId() const = 0;
        virtual uint32_t FcId() const = 0;

        virtual bool ProcessFrame(uint32_t canId, const hal::Can::Message& frame) = 0;
        virtual bool SendPdu(infra::ConstByteRange pdu, const infra::Function<void()>& onDone) = 0;

        virtual bool IsSenderIdle() const = 0;
        virtual bool IsReceiverIdle() const = 0;

    protected:
        IsoTpChannel() = default;
        IsoTpChannel(const IsoTpChannel&) = delete;
        IsoTpChannel& operator=(const IsoTpChannel&) = delete;
        ~IsoTpChannel() = default;
    };
}
