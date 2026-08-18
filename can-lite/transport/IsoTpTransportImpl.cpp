#include "can-lite/transport/IsoTpTransportImpl.hpp"

namespace services
{
    bool IsoTpTransportImpl::RegisterReceiveChannel(uint32_t dataId, uint32_t fcId)
    {
        if (FindChannel(dataId) != nullptr)
            return false;
        if (FindChannel(fcId) != nullptr)
            return false;
        auto* slot = AllocateFreeChannel();
        if (slot == nullptr)
            return false;

        slot->Configure(dataId, fcId, [this](uint32_t cid, const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                return OnRawSend(cid, f, d);
            },
            [this](uint32_t did, infra::ConstByteRange pdu)
            {
                OnPduReady(did, pdu);
            },
            [this](uint32_t did, iso_tp::AbortReason r)
            {
                OnAbort(did, r);
            });
        return true;
    }

    void IsoTpTransportImpl::ReleaseChannel(uint32_t dataId)
    {
        auto* ch = FindChannel(dataId);
        if (ch != nullptr)
            ch->Release();
    }

    bool IsoTpTransportImpl::SendPdu(uint32_t dataId, uint32_t fcId,
        infra::ConstByteRange pdu, const infra::Function<void()>& onDone)
    {
        auto* ch = FindChannel(dataId);

        if (ch == nullptr)
        {
            ch = AllocateFreeChannel();
            if (ch == nullptr)
                return false;
            ch->Configure(dataId, fcId, [this](uint32_t cid, const hal::Can::Message& f, const infra::Function<void(bool)>& d)
                {
                    return OnRawSend(cid, f, d);
                },
                [this](uint32_t did, infra::ConstByteRange p)
                {
                    OnPduReady(did, p);
                },
                [this](uint32_t did, iso_tp::AbortReason r)
                {
                    OnAbort(did, r);
                });
        }

        // The channel stays occupied (and reusable for a subsequent send/receive
        // to the same dataId/fcId pair) once this transfer completes; it is
        // reclaimed via ReleaseChannel(), called on abort (see AttachIsoTpTransport)
        // or explicitly by the application once it's done with a dataId.
        return ch->SendPdu(pdu, onDone);
    }

    bool IsoTpTransportImpl::ProcessFrame(uint32_t canId, const hal::Can::Message& frame)
    {
        for (auto* ch : channelRange)
        {
            if (ch->IsOccupied() && ch->ProcessFrame(canId, frame))
                return true;
        }
        return false;
    }

    void IsoTpTransportImpl::SetOnPduReceived(
        infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> callback)
    {
        onPduReceived = callback;
    }

    void IsoTpTransportImpl::SetOnAbort(
        infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> callback)
    {
        onAbortCallback = callback;
    }

    iso_tp::IsoTpChannel* IsoTpTransportImpl::FindChannel(uint32_t canId)
    {
        for (auto* ch : channelRange)
        {
            if (ch->IsOccupied() && (ch->DataId() == canId || ch->FcId() == canId))
                return ch;
        }
        return nullptr;
    }

    iso_tp::IsoTpChannel* IsoTpTransportImpl::AllocateFreeChannel()
    {
        for (auto* ch : channelRange)
        {
            if (!ch->IsOccupied())
                return ch;
        }
        return nullptr;
    }

    bool IsoTpTransportImpl::OnRawSend(uint32_t canId,
        const hal::Can::Message& frame, const infra::Function<void(bool success)>& onDone)
    {
        return transport.SendRawFrame(hal::Can::Id::Create29BitId(canId), frame, onDone);
    }

    void IsoTpTransportImpl::OnPduReady(uint32_t dataId, infra::ConstByteRange pdu) const
    {
        if (onPduReceived)
            onPduReceived(dataId, pdu);
    }

    void IsoTpTransportImpl::OnAbort(uint32_t dataId, iso_tp::AbortReason reason) const
    {
        if (onAbortCallback)
            onAbortCallback(dataId, reason);
    }
}
