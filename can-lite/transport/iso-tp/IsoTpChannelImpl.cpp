#include "can-lite/transport/iso-tp/IsoTpChannelImpl.hpp"
#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"

namespace services::iso_tp
{
    void IsoTpChannelImpl::Configure(uint32_t newRxId, uint32_t newTxId,
        RawSendFunc rawSend, PduReadyFunc onPduReady, AbortFunc onAbort)
    {
        rxId = newRxId;
        txId = newTxId;
        occupied = true;
        receptionEnabled = false;
        rawSendFunc = rawSend;
        onPduReadyFunc = onPduReady;
        onAbortFunc = onAbort;

        sender.Configure(
            [this](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                Transmit(f, d);
            },
            [this](AbortReason r)
            {
                NotifyAbort(txId, r);
            });

        receiver.Configure(
            [this](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                Transmit(f, d);
            },
            [this](infra::ConstByteRange pdu)
            {
                NotifyPduReady(pdu);
            },
            [this](AbortReason r)
            {
                NotifyAbort(rxId, r);
            });
    }

    void IsoTpChannelImpl::EnableReception()
    {
        receptionEnabled = true;
    }

    void IsoTpChannelImpl::Release()
    {
        sender.Reset();
        receiver.Reset();
        occupied = false;
        receptionEnabled = false;
    }

    bool IsoTpChannelImpl::IsOccupied() const
    {
        return occupied;
    }

    bool IsoTpChannelImpl::IsReceptionEnabled() const
    {
        return receptionEnabled;
    }

    uint32_t IsoTpChannelImpl::RxId() const
    {
        return rxId;
    }

    uint32_t IsoTpChannelImpl::TxId() const
    {
        return txId;
    }

    bool IsoTpChannelImpl::ProcessFrame(uint32_t canId, const hal::Can::Message& frame)
    {
        if (canId != rxId)
            return false;

        if (IsoTpFrameCodec::DecodeFrameType(frame) == FrameType::flowControl)
        {
            sender.ProcessFlowControl(frame);
            return true;
        }

        if (!receptionEnabled)
            return false;

        receiver.ProcessFrame(frame);
        return true;
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

    void IsoTpChannelImpl::Transmit(const hal::Can::Message& frame, const infra::Function<void(bool success)>& onDone) const
    {
        if (!rawSendFunc(txId, frame, onDone))
            onDone(false);
    }

    void IsoTpChannelImpl::NotifyPduReady(infra::ConstByteRange pdu) const
    {
        onPduReadyFunc(rxId, pdu);
    }

    void IsoTpChannelImpl::NotifyAbort(uint32_t dataId, AbortReason reason) const
    {
        onAbortFunc(dataId, reason);
    }
}
