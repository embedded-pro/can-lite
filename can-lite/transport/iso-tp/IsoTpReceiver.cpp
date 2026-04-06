#include "can-lite/transport/iso-tp/IsoTpReceiver.hpp"
#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"

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
                break;
            case unknown:
                Abort(AbortReason::unexpectedFrame);
                break;
        }
    }

    bool IsoTpReceiver::IsIdle() const
    {
        return state == ReceiverState::idle;
    }

    void IsoTpReceiver::HandleSingleFrame(const hal::Can::Message& frame)
    {
        if (state != ReceiverState::idle)
        {
            nCrTimer.Cancel();
            state = ReceiverState::idle;
            assemblyBuffer.clear();
        }

        uint8_t len = IsoTpFrameCodec::DecodeSingleFrameLength(frame);
        if (len == 0u || len > sfMaxPayloadBytes || len > static_cast<uint8_t>(frame.size() - 1u))
        {
            Abort(AbortReason::unexpectedFrame);
            return;
        }
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
        {
            Abort(AbortReason::unexpectedFrame);
            return;
        }

        uint16_t len = IsoTpFrameCodec::DecodeFirstFrameTotalLength(frame);
        if (len <= sfMaxPayloadBytes)
        {
            Abort(AbortReason::unexpectedFrame);
            return;
        }
        if (len > assemblyBuffer.max_size())
        {
            hal::Can::Message fc;
            IsoTpFrameCodec::EncodeFlowControl(FlowStatus::overflow, 0u, 0u, fc);
            if (sendFcFunc)
                sendFcFunc(fc, []() {});
            Abort(AbortReason::overflow);
            return;
        }

        expectedTotalLength = len;
        expectedSn = 1u;
        assemblyBuffer.clear();

        for (uint8_t i = ffFirstDataOffset; i < static_cast<uint8_t>(frame.size()); ++i)
            assemblyBuffer.push_back(frame[i]);

        state = ReceiverState::receivingCf;
        nCrTimer.Start(nCrTimeout, [this]()
            {
                Abort(AbortReason::nCrTimeout);
            });

        SendCtsFlowControl();
    }

    void IsoTpReceiver::HandleConsecutiveFrame(const hal::Can::Message& frame)
    {
        if (state != ReceiverState::receivingCf)
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
    }

    void IsoTpReceiver::SendCtsFlowControl() const
    {
        hal::Can::Message fc;
        IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0u, fc);
        if (sendFcFunc)
            sendFcFunc(fc, []() {});
    }

    void IsoTpReceiver::Abort(AbortReason reason)
    {
        nCrTimer.Cancel();
        assemblyBuffer.clear();
        state = ReceiverState::idle;
        if (onAbortFunc)
            onAbortFunc(reason);
    }
}
