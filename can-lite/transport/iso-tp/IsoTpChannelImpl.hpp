#pragma once

#include "can-lite/transport/iso-tp/IsoTpChannel.hpp"
#include "can-lite/transport/iso-tp/IsoTpReceiver.hpp"
#include "can-lite/transport/iso-tp/IsoTpSender.hpp"
#include "can-lite/transport/iso-tp/IsoTpTypes.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace services::iso_tp
{
    namespace detail
    {
        template<uint16_t MaxPduSize>
        struct IsoTpChannelStorage
        {
            typename IsoTpSender::template WithStorage<MaxPduSize> senderStorage;
            typename IsoTpReceiver::template WithStorage<MaxPduSize> receiverStorage;
        };
    }

    class IsoTpChannelImpl
        : public IsoTpChannel
    {
    public:
        template<uint16_t MaxPduSize>
        class WithStorage;

        IsoTpChannelImpl(IsoTpSender& sender, IsoTpReceiver& receiver);

        void Configure(uint32_t newRxId, uint32_t newTxId,
            RawSendFunc rawSend, PduReadyFunc onPduReady, AbortFunc onAbort) override;
        void EnableReception() override;
        void SetFlowControlParameters(uint8_t blockSize, uint8_t separationTime) override;

        void Release() override;

        bool IsOccupied() const override;
        bool IsReceptionEnabled() const override;
        uint32_t RxId() const override;
        uint32_t TxId() const override;

        bool ProcessFrame(uint32_t canId, const hal::Can::Message& frame) override;
        bool SendPdu(infra::ConstByteRange pdu, const infra::Function<void()>& onDone) override;

        bool IsSenderIdle() const override;
        bool IsReceiverIdle() const override;

    private:
        void Transmit(const hal::Can::Message& frame, const infra::Function<void(bool success)>& onDone) const;
        void NotifyPduReady(infra::ConstByteRange pdu) const;
        void NotifyAbort(uint32_t dataId, AbortReason reason) const;

        uint32_t rxId{ 0u };
        uint32_t txId{ 0u };
        bool occupied{ false };
        bool receptionEnabled{ false };

        RawSendFunc rawSendFunc;
        PduReadyFunc onPduReadyFunc;
        AbortFunc onAbortFunc;

        IsoTpSender& sender;
        IsoTpReceiver& receiver;
    };

    template<uint16_t MaxPduSize>
    class IsoTpChannelImpl::WithStorage
        : private detail::IsoTpChannelStorage<MaxPduSize>
        , public IsoTpChannelImpl
    {
    public:
        WithStorage()
            : IsoTpChannelImpl(this->senderStorage, this->receiverStorage)
        {}
    };
}
