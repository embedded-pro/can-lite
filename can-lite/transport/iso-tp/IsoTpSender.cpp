#include "can-lite/transport/iso-tp/IsoTpSender.hpp"
#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"

namespace services::iso_tp
{
    IsoTpSender::IsoTpSender(infra::BoundedVector<uint8_t>& pduBuffer)
        : pduBuffer(pduBuffer)
    {}

    void IsoTpSender::Configure(SendFrameFunc sendFrame, AbortFunc onAbort)
    {
        sendFrameFunc = sendFrame;
        onAbortFunc = onAbort;
    }

    bool IsoTpSender::Send(infra::ConstByteRange pdu, const infra::Function<void()>& onDone)
    {
        if (pdu.empty())
            return false;
        if (pdu.size() > 0x0FFFu)
            return false;
        if (state != SenderState::idle)
            return false;
        if (pdu.size() > pduBuffer.max_size())
            return false;

        pduBuffer.assign(pdu.begin(), pdu.end());
        onDoneCallback = onDone;
        bytesSent = 0;
        sequenceNumber = 1;
        blockSize = 0;
        blocksRemaining = 0;
        stMinByte = 0;

        SendFirstOrSingleFrame();
        return true;
    }

    void IsoTpSender::ProcessFlowControl(const hal::Can::Message& frame)
    {
        if (state != SenderState::waitingForFc)
            return;

        FlowStatus fs;
        uint8_t bs;
        uint8_t stMin;
        IsoTpFrameCodec::DecodeFlowControl(frame, fs, bs, stMin);

        if (fs == FlowStatus::overflow || fs == FlowStatus::wait)
        {
            Abort(AbortReason::unexpectedFrame);
            return;
        }

        nBsTimer.Cancel();
        blockSize = bs;
        blocksRemaining = bs;
        stMinByte = stMin;
        state = SenderState::sendingCf;
        ScheduleNextCf();
    }

    bool IsoTpSender::IsIdle() const
    {
        return state == SenderState::idle;
    }

    void IsoTpSender::SendFirstOrSingleFrame()
    {
        hal::Can::Message frame;

        if (pduBuffer.size() <= sfMaxPayloadBytes)
        {
            IsoTpFrameCodec::EncodeSingleFrame(infra::MakeRange(pduBuffer), frame);
            state = SenderState::sendingSf;
            sendFrameFunc(frame, [this]()
                {
                    state = SenderState::idle;
                    auto done = onDoneCallback;
                    done();
                });
        }
        else
        {
            IsoTpFrameCodec::EncodeFirstFrame(infra::MakeRange(pduBuffer), frame);
            bytesSent = ffFirstDataBytes;
            state = SenderState::waitingForFc;
            sendFrameFunc(frame, [this]()
                {
                    nBsTimer.Start(nBsTimeout, [this]()
                        {
                            Abort(AbortReason::nBsTimeout);
                        });
                });
        }
    }

    void IsoTpSender::ScheduleNextCf()
    {
        auto delay = IsoTpFrameCodec::StMinToDuration(stMinByte);
        if (delay > infra::Duration{})
        {
            stMinTimer.Start(delay, [this]()
                {
                    SendNextConsecutiveFrame();
                });
        }
        else
        {
            SendNextConsecutiveFrame();
        }
    }

    void IsoTpSender::SendNextConsecutiveFrame()
    {
        infra::ConstByteRange remaining = infra::DiscardHead(infra::MakeRange(pduBuffer), bytesSent);
        hal::Can::Message frame;
        uint8_t written = IsoTpFrameCodec::EncodeConsecutiveFrame(sequenceNumber & snMask, remaining, frame);

        sequenceNumber = static_cast<uint8_t>((sequenceNumber + 1u) & 0x0Fu);
        bytesSent = static_cast<uint16_t>(bytesSent + written);

        bool allSent = (bytesSent >= static_cast<uint16_t>(pduBuffer.size()));

        if (!allSent && blockSize != 0u)
        {
            if (blocksRemaining > 0u)
                --blocksRemaining;

            if (blocksRemaining == 0u)
            {
                state = SenderState::waitingForFc;
                sendFrameFunc(frame, [this]()
                    {
                        nBsTimer.Start(nBsTimeout, [this]()
                            {
                                Abort(AbortReason::nBsTimeout);
                            });
                    });
                return;
            }
        }

        sendFrameFunc(frame, [this, allSent]()
            {
                if (allSent)
                {
                    state = SenderState::idle;
                    auto done = onDoneCallback;
                    done();
                }
                else if (state == SenderState::sendingCf)
                {
                    ScheduleNextCf();
                }
            });
    }

    void IsoTpSender::Abort(AbortReason reason)
    {
        nBsTimer.Cancel();
        stMinTimer.Cancel();
        state = SenderState::idle;
        if (onAbortFunc)
            onAbortFunc(reason);
    }
}
