#pragma once

#include "can-lite/transport/iso-tp/IsoTpChannel.hpp"
#include "can-lite/transport/iso-tp/IsoTpReceiver.hpp"
#include "can-lite/transport/iso-tp/IsoTpSender.hpp"
#include "can-lite/transport/iso-tp/IsoTpTypes.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/WithStorage.hpp"
#include <cstdint>

namespace services::iso_tp
{
    class IsoTpChannelImpl
        : public IsoTpChannel
    {
    public:
        template<uint16_t MaxPduSize>
        struct Storage
        {
            typename IsoTpSender::template WithStorage<MaxPduSize> sender;
            typename IsoTpReceiver::template WithStorage<MaxPduSize> receiver;
        };

        template<uint16_t MaxPduSize>
        using WithStorage = infra::WithStorage<IsoTpChannelImpl, Storage<MaxPduSize>>;

        template<uint16_t MaxPduSize>
        explicit IsoTpChannelImpl(Storage<MaxPduSize>& storage);

        void Configure(uint32_t newDataId, uint32_t newFcId,
            RawSendFunc rawSend, PduReadyFunc onPduReady, AbortFunc onAbort) override;

        void Release() override;

        bool IsOccupied() const override;
        uint32_t DataId() const override;
        uint32_t FcId() const override;

        bool ProcessFrame(uint32_t canId, const hal::Can::Message& frame) override;
        bool SendPdu(infra::ConstByteRange pdu, const infra::Function<void()>& onDone) override;

        bool IsSenderIdle() const override;
        bool IsReceiverIdle() const override;

    private:
        void SendToDataId(const hal::Can::Message& frame, const infra::Function<void(bool success)>& onDone);
        void SendToFcId(const hal::Can::Message& frame, const infra::Function<void(bool success)>& onDone);
        void NotifyPduReady(infra::ConstByteRange pdu) const;
        void NotifyAbort(AbortReason reason) const;

        uint32_t dataId{ 0u };
        uint32_t fcId{ 0u };
        bool occupied{ false };

        RawSendFunc rawSendFunc;
        PduReadyFunc onPduReadyFunc;
        AbortFunc onAbortFunc;

        IsoTpSender& sender;
        IsoTpReceiver& receiver;
    };

    template<uint16_t MaxPduSize>
    IsoTpChannelImpl::IsoTpChannelImpl(Storage<MaxPduSize>& storage)
        : sender(storage.sender)
        , receiver(storage.receiver)
    {}
}
