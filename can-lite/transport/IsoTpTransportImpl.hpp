#pragma once

#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/transport/IsoTpTransport.hpp"
#include "can-lite/transport/iso-tp/IsoTpChannelImpl.hpp"
#include "can-lite/transport/iso-tp/IsoTpTypes.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace services
{
    namespace detail
    {
        template<uint16_t MaxPduSize, uint8_t MaxChannels>
        struct IsoTpTransportStorage
        {
            IsoTpTransportStorage()
            {
                for (std::size_t i = 0; i != MaxChannels; ++i)
                    channelPointers[i] = &channels[i];
            }

            IsoTpTransportStorage(const IsoTpTransportStorage&) = delete;
            IsoTpTransportStorage& operator=(const IsoTpTransportStorage&) = delete;

            std::array<typename iso_tp::IsoTpChannelImpl::template WithStorage<MaxPduSize>, MaxChannels> channels;
            std::array<iso_tp::IsoTpChannel*, MaxChannels> channelPointers{};
        };
    }

    class IsoTpTransportImpl
        : public IsoTpTransport
    {
    public:
        template<uint16_t MaxPduSize, uint8_t MaxChannels = 4>
        class WithStorage;

        IsoTpTransportImpl(infra::MemoryRange<iso_tp::IsoTpChannel*> channels, CanFrameTransport& transport);

        bool RegisterReceiveChannel(uint32_t dataId, uint32_t fcId) override;

        void ReleaseChannel(uint32_t dataId) override;

        bool SendPdu(uint32_t dataId, uint32_t fcId,
            infra::ConstByteRange pdu,
            const infra::Function<void()>& onDone) override;

        bool ProcessFrame(uint32_t canId, const hal::Can::Message& frame) override;

        void SetOnPduReceived(
            infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> callback) override;

        void SetOnAbort(
            infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> callback) override;

    private:
        iso_tp::IsoTpChannel* FindChannel(uint32_t canId);
        iso_tp::IsoTpChannel* FindConnection(uint32_t rxId, uint32_t txId);
        iso_tp::IsoTpChannel* OpenConnection(uint32_t rxId, uint32_t txId);
        iso_tp::IsoTpChannel* AllocateFreeChannel();

        bool OnRawSend(uint32_t canId, const hal::Can::Message& frame,
            const infra::Function<void(bool success)>& onDone);
        void OnPduReady(uint32_t dataId, infra::ConstByteRange pdu) const;
        void OnAbort(uint32_t dataId, iso_tp::AbortReason reason) const;

        infra::MemoryRange<iso_tp::IsoTpChannel*> channels;
        CanFrameTransport& transport;
        infra::Function<void(uint32_t, infra::ConstByteRange)> onPduReceived;
        infra::Function<void(uint32_t, iso_tp::AbortReason)> onAbortCallback;
    };

    template<uint16_t MaxPduSize, uint8_t MaxChannels>
    class IsoTpTransportImpl::WithStorage
        : private detail::IsoTpTransportStorage<MaxPduSize, MaxChannels>
        , public IsoTpTransportImpl
    {
    public:
        explicit WithStorage(CanFrameTransport& transport)
            : IsoTpTransportImpl(infra::MakeRange(this->channelPointers), transport)
        {}
    };
}
