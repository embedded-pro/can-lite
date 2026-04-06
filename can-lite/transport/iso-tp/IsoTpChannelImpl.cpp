#include "can-lite/transport/iso-tp/IsoTpChannelImpl.hpp"
#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"

namespace services::iso_tp
{
    void IsoTpChannelImpl::Configure(uint32_t dataId, uint32_t fcId,
        RawSendFunc rawSend, PduReadyFunc onPduReady, AbortFunc onAbort)
    {
        dataId_ = dataId;
        fcId_ = fcId;
        occupied_ = true;
        rawSend_ = rawSend;
        onPduReady_ = onPduReady;
        onAbort_ = onAbort;

        sender_.Configure(
            [this](const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                SendToDataId(f, d);
            },
            [this](AbortReason r)
            {
                NotifyAbort(r);
            });

        receiver_.Configure(
            [this](const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                SendToFcId(f, d);
            },
            [this](infra::ConstByteRange pdu)
            {
                NotifyPduReady(pdu);
            },
            [this](AbortReason r)
            {
                NotifyAbort(r);
            });
    }

    void IsoTpChannelImpl::Release()
    {
        occupied_ = false;
    }

    bool IsoTpChannelImpl::IsOccupied() const
    {
        return occupied_;
    }

    uint32_t IsoTpChannelImpl::DataId() const
    {
        return dataId_;
    }

    uint32_t IsoTpChannelImpl::FcId() const
    {
        return fcId_;
    }

    bool IsoTpChannelImpl::ProcessFrame(uint32_t canId, const hal::Can::Message& frame)
    {
        if (canId == fcId_ &&
            IsoTpFrameCodec::DecodeFrameType(frame) == FrameType::flowControl)
        {
            sender_.ProcessFlowControl(frame);
            return true;
        }
        if (canId == dataId_)
        {
            auto type = IsoTpFrameCodec::DecodeFrameType(frame);
            if (type != FrameType::flowControl)
            {
                receiver_.ProcessFrame(frame);
                return true;
            }
        }
        return false;
    }

    bool IsoTpChannelImpl::SendPdu(infra::ConstByteRange pdu, const infra::Function<void()>& onDone)
    {
        return sender_.Send(pdu, onDone);
    }

    bool IsoTpChannelImpl::IsSenderIdle() const
    {
        return sender_.IsIdle();
    }

    bool IsoTpChannelImpl::IsReceiverIdle() const
    {
        return receiver_.IsIdle();
    }

    void IsoTpChannelImpl::SendToDataId(const hal::Can::Message& frame, const infra::Function<void()>& onDone)
    {
        if (!rawSend_(dataId_, frame, onDone))
            NotifyAbort(AbortReason::unexpectedFrame);
    }

    void IsoTpChannelImpl::SendToFcId(const hal::Can::Message& frame, const infra::Function<void()>& onDone)
    {
        if (!rawSend_(fcId_, frame, onDone))
            NotifyAbort(AbortReason::unexpectedFrame);
    }

    void IsoTpChannelImpl::NotifyPduReady(infra::ConstByteRange pdu)
    {
        onPduReady_(dataId_, pdu);
    }

    void IsoTpChannelImpl::NotifyAbort(AbortReason reason)
    {
        onAbort_(dataId_, reason);
    }
}
