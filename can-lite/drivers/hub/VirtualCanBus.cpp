#include "can-lite/drivers/hub/VirtualCanBus.hpp"
#include <algorithm>
#include <chrono>

namespace services
{
    namespace
    {
        constexpr const char* accepted{ "\r" };
        constexpr const char* refused{ "\a" };
        constexpr uint32_t standardFrameBits{ 47 };
        constexpr uint32_t extendedFrameBits{ 67 };
        constexpr uint32_t standardStuffableBits{ 34 };
        constexpr uint32_t extendedStuffableBits{ 54 };
        constexpr uint32_t bitsPerStuffBit{ 4 };
        constexpr uint64_t nanosecondsPerSecond{ 1000000000 };

        infra::Duration TransmissionTime(const SlcanFrame& frame, uint32_t bitrate)
        {
            const auto dataBits = static_cast<uint32_t>(8 * frame.data.size());
            const auto stuffable = (frame.extended ? extendedStuffableBits : standardStuffableBits) + dataBits;
            const auto bits = (frame.extended ? extendedFrameBits : standardFrameBits) + dataBits + (stuffable - 1) / bitsPerStuffBit;
            return std::chrono::duration_cast<infra::Duration>(std::chrono::nanoseconds(uint64_t{ bits } * nanosecondsPerSecond / bitrate));
        }
    }

    bool VirtualCanBus::Attach(VirtualCanParticipant& participant)
    {
        for (auto& member : members)
            if (member.participant == nullptr)
            {
                member.participant = &participant;
                member.decoder.Reset();
                member.bus.clear();
                member.bitrate = 0;
                member.open = false;
                return true;
            }

        return false;
    }

    void VirtualCanBus::Detach(const VirtualCanParticipant& participant)
    {
        auto* member = Find(participant);
        if (member == nullptr)
            return;

        auto* bus = BusOf(*member);
        member->participant = nullptr;
        member->bus.clear();
        if (bus == nullptr)
            return;

        const auto left = std::ranges::remove_if(bus->queue, [&participant](const Pending& pending)
            {
                return pending.sender == &participant;
            });
        bus->queue.erase(left.begin(), left.end());

        const bool joined = std::ranges::any_of(members, [bus](const Member& other)
            {
                return other.participant != nullptr && other.bus == bus->name;
            });
        if (!joined)
        {
            bus->timer.Cancel();
            bus->queue.clear();
            bus->onWire.reset();
            bus->name.clear();
        }
    }

    void VirtualCanBus::Receive(const VirtualCanParticipant& participant, infra::ConstByteRange bytes)
    {
        for (const auto byte : bytes)
        {
            auto* member = Find(participant);
            if (member == nullptr)
                return;

            if (const auto item = member->decoder.Feed(static_cast<char>(byte)))
                Handle(*member, *item);
        }
    }

    VirtualCanBus::Member* VirtualCanBus::Find(const VirtualCanParticipant& participant)
    {
        for (auto& member : members)
            if (member.participant == &participant)
                return &member;
        return nullptr;
    }

    void VirtualCanBus::Handle(Member& member, const SlcanCommandDecoder::Item& item)
    {
        using enum SlcanCommandDecoder::Command;

        switch (item.command)
        {
            case join:
                if (!Join(member, item.name))
                    member.participant->Write(refused);
                break;
            case close:
                member.open = false;
                member.participant->Write(accepted);
                break;
            case bitrate:
                member.participant->Write(SelectBitrate(member, item.bitrate) ? accepted : refused);
                break;
            case open:
                member.participant->Write(OpenChannel(member) ? accepted : refused);
                break;
            case frame:
                if (!Queue(member, item.frame))
                    member.participant->Write(refused);
                break;
            default:
                member.participant->Write(refused);
                break;
        }
    }

    bool VirtualCanBus::Join(Member& member, infra::BoundedConstString name)
    {
        if (!member.bus.empty() || AcquireBus(name) == nullptr)
            return false;

        member.bus = name;
        return true;
    }

    bool VirtualCanBus::SelectBitrate(Member& member, uint32_t bitrate) const
    {
        if (member.bus.empty() || ConflictsWithOpenMembers(member, bitrate))
            return false;

        member.bitrate = bitrate;
        return true;
    }

    bool VirtualCanBus::OpenChannel(Member& member)
    {
        if (member.bus.empty() || member.bitrate == 0 || ConflictsWithOpenMembers(member, member.bitrate))
            return false;

        member.open = true;
        BusOf(member)->bitrate = member.bitrate;
        return true;
    }

    bool VirtualCanBus::Queue(const Member& member, const SlcanFrame& frame)
    {
        auto* bus = BusOf(member);
        if (!member.open || frame.remote || bus == nullptr || bus->queue.full())
            return false;

        bus->queue.push_back(Pending{ member.participant, frame });
        if (!bus->onWire.has_value())
            StartNext(*bus, infra::Now());
        return true;
    }

    bool VirtualCanBus::ConflictsWithOpenMembers(const Member& member, uint32_t bitrate) const
    {
        return std::ranges::any_of(members, [&member, bitrate](const Member& other)
            {
                return &other != &member && other.participant != nullptr && other.open && other.bus == member.bus && other.bitrate != bitrate;
            });
    }

    VirtualCanBus::Bus* VirtualCanBus::BusOf(const Member& member)
    {
        if (member.bus.empty())
            return nullptr;

        for (auto& bus : buses)
            if (bus.name == member.bus)
                return &bus;
        return nullptr;
    }

    VirtualCanBus::Bus* VirtualCanBus::AcquireBus(infra::BoundedConstString name)
    {
        Bus* free{};
        for (auto& bus : buses)
        {
            if (bus.name == name)
                return &bus;
            if (free == nullptr && bus.name.empty())
                free = &bus;
        }

        if (free != nullptr)
            free->name = name;
        return free;
    }

    void VirtualCanBus::StartNext(Bus& bus, infra::TimePoint start)
    {
        while (!bus.queue.empty())
        {
            const auto next = NextToWin(bus);
            const auto end = start + TransmissionTime(next->frame, bus.bitrate);
            const auto now = infra::Now();
            if (end > now)
            {
                bus.onWire = *next;
                bus.queue.erase(next);
                bus.onWireEnd = end;
                bus.timer.Start(end - now, [this, &bus]()
                    {
                        Deliver(bus);
                    });
                return;
            }

            Transmit(bus, *next);
            bus.queue.erase(next);
            start = end;
        }
    }

    VirtualCanBus::Pending* VirtualCanBus::NextToWin(Bus& bus)
    {
        Pending* winner{};
        for (auto candidate = bus.queue.begin(); candidate != bus.queue.end(); ++candidate)
        {
            const bool headOfItsSender = std::none_of(bus.queue.begin(), candidate, [candidate](const Pending& earlier)
                {
                    return earlier.sender == candidate->sender;
                });
            if (headOfItsSender && (winner == nullptr || candidate->frame.id < winner->frame.id))
                winner = candidate;
        }
        return winner;
    }

    void VirtualCanBus::Deliver(Bus& bus)
    {
        const auto pending = *bus.onWire;
        bus.onWire.reset();
        Transmit(bus, pending);
        StartNext(bus, bus.onWireEnd);
    }

    void VirtualCanBus::Transmit(const Bus& bus, const Pending& pending)
    {
        const auto encoded = EncodeSlcanFrame(pending.frame.id, pending.frame.extended, pending.frame.data);

        for (auto& member : members)
            if (member.participant != nullptr && member.participant != pending.sender && member.open && member.bus == bus.name)
                member.participant->Write(encoded);
    }
}
