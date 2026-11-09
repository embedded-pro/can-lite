#pragma once

#include "can-lite/drivers/codec/SlcanCommandDecoder.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/ByteRange.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    class VirtualCanParticipant
    {
    public:
        VirtualCanParticipant() = default;
        VirtualCanParticipant(const VirtualCanParticipant&) = delete;
        VirtualCanParticipant& operator=(const VirtualCanParticipant&) = delete;

        virtual void Write(infra::BoundedConstString bytes) = 0;

    protected:
        ~VirtualCanParticipant() = default;
    };

    class VirtualCanBus
    {
    public:
        static constexpr std::size_t maxParticipants{ 16 };
        static constexpr std::size_t maxBuses{ 4 };
        static constexpr std::size_t maxQueuedFrames{ 64 };

        bool Attach(VirtualCanParticipant& participant);
        void Detach(const VirtualCanParticipant& participant);
        void Receive(const VirtualCanParticipant& participant, infra::ConstByteRange bytes);

    private:
        struct Member
        {
            VirtualCanParticipant* participant{};
            SlcanCommandDecoder decoder;
            VirtualBusName bus;
            uint32_t bitrate{};
            bool open{};
        };

        struct Pending
        {
            const VirtualCanParticipant* sender{};
            SlcanFrame frame;
        };

        struct Bus
        {
            VirtualBusName name;
            uint32_t bitrate{};
            infra::BoundedVector<Pending>::WithMaxSize<maxQueuedFrames> queue;
            std::optional<Pending> onWire;
            infra::TimerSingleShot timer;
        };

        Member* Find(const VirtualCanParticipant& participant);
        void Handle(Member& member, const SlcanCommandDecoder::Item& item);
        bool Join(Member& member, infra::BoundedConstString name);
        bool SelectBitrate(Member& member, uint32_t bitrate) const;
        bool OpenChannel(Member& member);
        bool Queue(const Member& member, const SlcanFrame& frame);
        bool ConflictsWithOpenMembers(const Member& member, uint32_t bitrate) const;
        Bus* BusOf(const Member& member);
        Bus* AcquireBus(infra::BoundedConstString name);
        void StartNext(Bus& bus);
        void Deliver(Bus& bus);

        std::array<Member, maxParticipants> members;
        std::array<Bus, maxBuses> buses;
    };
}
