#include "can-lite/transport/iso-tp/IsoTpReceiver.hpp"
#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"
#include <algorithm>

namespace services::iso_tp
{
    IsoTpReceiver::IsoTpReceiver(infra::BoundedVector<uint8_t>& assemblyBuffer)
        : assemblyBuffer(assemblyBuffer)
    {}

    void IsoTpReceiver::Configure(SendFcFunc sendFc, PduReadyFunc onPduReady, AbortFunc onAbort)
    {
        sendFcFunc = sendFc;
        onPduReadyFunc = onPduReady;
        onAbortFunc = onAbort;
    }

    void IsoTpReceiver::SetFlowControlParameters(uint8_t newBlockSize, uint8_t newSeparationTime)
    {
        blockSize = newBlockSize;
        separationTime = newSeparationTime;
    }

    void IsoTpReceiver::ProcessFrame(const hal::Can::Message& frame)
    {
        using enum FrameType;
        switch (IsoTpFrameCodec::DecodeFrameType(frame))
        {
            case singleFrame:
                HandleSingleFrame(frame);
                break;
            case firstFrame:
                HandleFirstFrame(frame);
                break;
            case consecutiveFrame:
                HandleConsecutiveFrame(frame);
                break;
            case flowControl:
            case unknown:
                break;
        }
    }

    void IsoTpReceiver::Reset()
    {
        Clear();
    }

    bool IsoTpReceiver::IsIdle() const
    {
        return state == ReceiverState::idle;
    }

    void IsoTpReceiver::HandleSingleFrame(const hal::Can::Message& frame)
    {
        uint8_t len = IsoTpFrameCodec::DecodeSingleFrameLength(frame);
        if (len == 0u || len > sfMaxPayloadBytes || len > static_cast<uint8_t>(frame.size() - 1u))
            return;

        EndSupersededReception();

        if (len > assemblyBuffer.max_size())
        {
            Abort(AbortReason::overflow);
            return;
        }

        assemblyBuffer.assign(frame.begin() + 1, frame.begin() + 1 + len);
        if (onPduReadyFunc)
            onPduReadyFunc(infra::MakeRange(assemblyBuffer));
        assemblyBuffer.clear();
    }

    void IsoTpReceiver::HandleFirstFrame(const hal::Can::Message& frame)
    {
        if (frame.size() < 8u)
            return;

        uint16_t len = IsoTpFrameCodec::DecodeFirstFrameTotalLength(frame);
        if (len <= sfMaxPayloadBytes)
            return;

        EndSupersededReception();

        if (len > assemblyBuffer.max_size())
        {
            SendFlowControl(FlowStatus::overflow);
            Abort(AbortReason::overflow);
            return;
        }

        expectedTotalLength = len;
        expectedSn = 1u;
        framesInBlock = 0u;
        assemblyBuffer.clear();
        ++reception;

        for (uint8_t i = ffFirstDataOffset; i < static_cast<uint8_t>(frame.size()); ++i)
            assemblyBuffer.push_back(frame[i]);

        state = ReceiverState::receivingCf;
        nCrTimer.Start(nCrTimeout, [this]()
            {
                Abort(AbortReason::nCrTimeout);
            });

        SendFlowControl(FlowStatus::continueToSend);
    }

    void IsoTpReceiver::HandleConsecutiveFrame(const hal::Can::Message& frame)
    {
        if (state != ReceiverState::receivingCf || !HasConsecutiveFrameLength(frame))
            return;

        uint8_t sn = IsoTpFrameCodec::DecodeConsecutiveFrameSn(frame);
        if (sn != expectedSn)
        {
            Abort(AbortReason::unexpectedFrame);
            return;
        }

        expectedSn = static_cast<uint8_t>((expectedSn + 1u) & 0x0Fu);
        nCrTimer.Start(nCrTimeout, [this]()
            {
                Abort(AbortReason::nCrTimeout);
            });

        for (std::size_t i = 1u; i < frame.size(); ++i)
        {
            if (assemblyBuffer.size() >= expectedTotalLength)
                break;
            assemblyBuffer.push_back(frame[i]);
        }

        if (assemblyBuffer.size() >= expectedTotalLength)
        {
            nCrTimer.Cancel();
            state = ReceiverState::idle;
            if (onPduReadyFunc)
                onPduReadyFunc(infra::MakeRange(assemblyBuffer));
            assemblyBuffer.clear();
        }
        else
            ContinueBlock();
    }

    bool IsoTpReceiver::HasConsecutiveFrameLength(const hal::Can::Message& frame) const
    {
        auto remaining = static_cast<std::size_t>(expectedTotalLength) - assemblyBuffer.size();
        return frame.size() >= 1u + std::min<std::size_t>(remaining, cfMaxDataBytes);
    }

    void IsoTpReceiver::ContinueBlock()
    {
        if (blockSize == 0u || ++framesInBlock != blockSize)
            return;

        framesInBlock = 0u;
        SendFlowControl(FlowStatus::continueToSend);
    }

    void IsoTpReceiver::EndSupersededReception()
    {
        if (state != ReceiverState::idle)
            Abort(AbortReason::unexpectedFrame);
    }

    void IsoTpReceiver::SendFlowControl(FlowStatus status)
    {
        hal::Can::Message fc;
        IsoTpFrameCodec::EncodeFlowControl(status, blockSize, separationTime, fc);
        if (!sendFcFunc)
            return;

        sendFcFunc(fc, [this, status, current = reception](bool success)
            {
                if (!success && status == FlowStatus::continueToSend && current == reception && state == ReceiverState::receivingCf)
                    Abort(AbortReason::sendFailed);
            });
    }

    void IsoTpReceiver::Abort(AbortReason reason)
    {
        Clear();
        if (onAbortFunc)
            onAbortFunc(reason);
    }

    void IsoTpReceiver::Clear()
    {
        nCrTimer.Cancel();
        assemblyBuffer.clear();
        state = ReceiverState::idle;
        ++reception;
    }
}
