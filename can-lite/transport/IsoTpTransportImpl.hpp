#pragma once

#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/transport/IsoTpTransport.hpp"
#include "can-lite/transport/iso-tp/IsoTpChannelImpl.hpp"
#include "can-lite/transport/iso-tp/IsoTpTypes.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace services
{
    class IsoTpTransportImpl
        : public IsoTpTransport
    {
    public:
        template<uint16_t MaxPduSize, uint8_t MaxChannels = 4>
        using WithStorage = infra::WithStorage<IsoTpTransportImpl,
            typename infra::BoundedVector<typename iso_tp::IsoTpChannelImpl::template WithStorage<MaxPduSize>>::template WithMaxSize<MaxChannels>>;

        template<typename ChannelType>
        explicit IsoTpTransportImpl(infra::BoundedVector<ChannelType>& channels, CanFrameTransport& transport);

        bool RegisterReceiveChannel(uint32_t dataId, uint32_t fcId) override;

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
        iso_tp::IsoTpChannel* AllocateFreeChannel();

        bool OnRawSend(uint32_t canId, const hal::Can::Message& frame,
            const infra::Function<void()>& onDone);
        void OnPduReady(uint32_t dataId, infra::ConstByteRange pdu) const;
        void OnAbort(uint32_t dataId, iso_tp::AbortReason reason) const;

        infra::MemoryRange<iso_tp::IsoTpChannel*> channels_;
        CanFrameTransport& transport_;
        infra::Function<void(uint32_t, infra::ConstByteRange)> onPduReceived_;
        infra::Function<void(uint32_t, iso_tp::AbortReason)> onAbortCallback_;

        static constexpr uint8_t maxSupportedChannels = 16u;
        std::array<iso_tp::IsoTpChannel*, maxSupportedChannels> channelPtrs_{};
        uint8_t channelCount_ = 0u;
    };

    template<typename ChannelType>
    IsoTpTransportImpl::IsoTpTransportImpl(infra::BoundedVector<ChannelType>& channels, CanFrameTransport& transport)
        : transport_(transport)
    {
        while (channels.size() < channels.max_size())
            channels.emplace_back();

        really_assert(channels.max_size() <= maxSupportedChannels);

        channelCount_ = static_cast<uint8_t>(channels.size());
        for (std::size_t i = 0u; i < channelCount_; ++i)
            channelPtrs_[i] = &channels[i];

        channels_ = infra::MemoryRange<iso_tp::IsoTpChannel*>(channelPtrs_.data(), channelPtrs_.data() + channelCount_);
    }
}
