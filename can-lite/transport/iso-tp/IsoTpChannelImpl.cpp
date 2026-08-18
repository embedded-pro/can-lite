#include "can-lite/transport/iso-tp/IsoTpChannelImpl.hpp"
#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"

namespace services::iso_tp
{
    void IsoTpChannelImpl::Configure(uint32_t newDataId, uint32_t newFcId,
        RawSendFunc rawSend, PduReadyFunc onPduReady, AbortFunc onAbort)
    {
        dataId = newDataId;
        fcId = newFcId;
        occupied = true;
        rawSendFunc = rawSend;
        onPduReadyFunc = onPduReady;
        onAbortFunc = onAbort;

        sender.Configure(
            [this](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                SendToDataId(f, d);
            },
            [this](AbortReason r)
            {
                NotifyAbort(r);
            });

        receiver.Configure(
            [this](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
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
        occupied = false;
    }

    bool IsoTpChannelImpl::IsOccupied() const
    {
        return occupied;
    }

    uint32_t IsoTpChannelImpl::DataId() const
    {
        return dataId;
    }

    uint32_t IsoTpChannelImpl::FcId() const
    {
        return fcId;
    }

    bool IsoTpChannelImpl::ProcessFrame(uint32_t canId, const hal::Can::Message& frame)
    {
        if (canId == fcId &&
            IsoTpFrameCodec::DecodeFrameType(frame) == FrameType::flowControl)
        {
            sender.ProcessFlowControl(frame);
            return true;
        }
        if (canId == dataId)
        {
            auto type = IsoTpFrameCodec::DecodeFrameType(frame);
            if (type != FrameType::flowControl)
            {
                receiver.ProcessFrame(frame);
                return true;
            }
        }
        return false;
    }

    bool IsoTpChannelImpl::SendPdu(infra::ConstByteRange pdu, const infra::Function<void()>& onDone)
    {
        return sender.Send(pdu, onDone);
    }

    bool IsoTpChannelImpl::IsSenderIdle() const
    {
        return sender.IsIdle();
    }

    bool IsoTpChannelImpl::IsReceiverIdle() const
    {
        return receiver.IsIdle();
    }

    void IsoTpChannelImpl::SendToDataId(const hal::Can::Message& frame, const infra::Function<void(bool success)>& onDone)
    {
        if (!rawSendFunc(dataId, frame, onDone))
            onDone(false);
    }

    void IsoTpChannelImpl::SendToFcId(const hal::Can::Message& frame, const infra::Function<void(bool success)>& onDone)
    {
        if (!rawSendFunc(fcId, frame, onDone))
            onDone(false);
    }

    void IsoTpChannelImpl::NotifyPduReady(infra::ConstByteRange pdu) const
    {
        onPduReadyFunc(dataId, pdu);
    }

    void IsoTpChannelImpl::NotifyAbort(AbortReason reason) const
    {
        onAbortFunc(dataId, reason);
    }
}
