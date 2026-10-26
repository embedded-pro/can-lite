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
    class IsoTpReceiver
    {
    public:
        template<uint16_t MaxPduSize>
        using WithStorage = infra::WithStorage<IsoTpReceiver,
            typename infra::BoundedVector<uint8_t>::template WithMaxSize<MaxPduSize>>;

        using SendFcFunc = infra::Function<void(const hal::Can::Message& frame,
            const infra::Function<void(bool success)>& onDone)>;
        using PduReadyFunc = infra::Function<void(infra::ConstByteRange pdu)>;
        using AbortFunc = infra::Function<void(AbortReason reason)>;

        explicit IsoTpReceiver(infra::BoundedVector<uint8_t>& assemblyBuffer);

        void Configure(SendFcFunc sendFc, PduReadyFunc onPduReady, AbortFunc onAbort);
        void SetFlowControlParameters(uint8_t blockSize, uint8_t separationTime);
        void ProcessFrame(const hal::Can::Message& frame);
        void Reset();
        bool IsIdle() const;

    private:
        void HandleSingleFrame(const hal::Can::Message& frame);
        void HandleFirstFrame(const hal::Can::Message& frame);
        void HandleConsecutiveFrame(const hal::Can::Message& frame);
        void EndSupersededReception();
        bool HasConsecutiveFrameLength(const hal::Can::Message& frame) const;
        void ContinueBlock();
        void SendFlowControl(FlowStatus status);
        void Abort(AbortReason reason);
        void Clear();

        SendFcFunc sendFcFunc;
        PduReadyFunc onPduReadyFunc;
        AbortFunc onAbortFunc;

        ReceiverState state = ReceiverState::idle;
        uint16_t expectedTotalLength = 0;
        uint8_t expectedSn = 1;
        uint8_t reception = 0;
        uint8_t blockSize = 0;
        uint8_t separationTime = 0;
        uint8_t framesInBlock = 0;

        infra::BoundedVector<uint8_t>& assemblyBuffer;

        infra::TimerSingleShot nCrTimer;
    };
}
