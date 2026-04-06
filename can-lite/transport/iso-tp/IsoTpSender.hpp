#pragma once

#include "can-lite/transport/iso-tp/IsoTpTypes.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/WithStorage.hpp"
#include <cstdint>

namespace services::iso_tp
{
    class IsoTpSender
    {
    public:
        template<uint16_t MaxPduSize>
        using WithStorage = infra::WithStorage<IsoTpSender,
            typename infra::BoundedVector<uint8_t>::template WithMaxSize<MaxPduSize>>;

        using SendFrameFunc = infra::Function<void(const hal::Can::Message& frame,
            const infra::Function<void()>& onDone)>;
        using AbortFunc = infra::Function<void(AbortReason reason)>;

        explicit IsoTpSender(infra::BoundedVector<uint8_t>& pduBuffer);

        void Configure(SendFrameFunc sendFrame, AbortFunc onAbort);

        bool Send(infra::ConstByteRange pdu, const infra::Function<void()>& onDone);
        void ProcessFlowControl(const hal::Can::Message& frame);
        bool IsIdle() const;

    private:
        void SendFirstOrSingleFrame();
        void SendNextConsecutiveFrame();
        void ScheduleNextCf();
        void Abort(AbortReason reason);

        SendFrameFunc sendFrameFunc;
        AbortFunc onAbortFunc;

        SenderState state = SenderState::idle;
        uint16_t bytesSent = 0;
        uint8_t sequenceNumber = 1;
        uint8_t blockSize = 0;
        uint8_t blocksRemaining = 0;
        uint8_t stMinByte = 0;

        infra::Function<void()> onDoneCallback;

        infra::BoundedVector<uint8_t>& pduBuffer;

        infra::TimerSingleShot nBsTimer;
        infra::TimerSingleShot stMinTimer;
    };
}
