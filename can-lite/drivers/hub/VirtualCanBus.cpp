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
            return std::chrono::nanoseconds(uint64_t{ bits } * nanosecondsPerSecond / bitrate);
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

    void VirtualCanBus::Detach(VirtualCanParticipant& participant)
    {
        auto* member = Find(participant);
        if (member == nullptr)
            return;

        auto* bus = BusOf(*member);
        member->participant = nullptr;
        member->bus.clear();
        if (bus == nullptr)
            return;

        bus->queue.erase(std::remove_if(bus->queue.begin(), bus->queue.end(), [&participant](const Pending& pending)
                             {
                                 return pending.sender == &participant;
                             }),
            bus->queue.end());

        const bool joined = std::any_of(members.begin(), members.end(), [bus](const Member& other)
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

    void VirtualCanBus::Receive(VirtualCanParticipant& participant, infra::ConstByteRange bytes)
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

    bool VirtualCanBus::SelectBitrate(Member& member, uint32_t bitrate)
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

    bool VirtualCanBus::Queue(Member& member, const SlcanFrame& frame)
    {
        auto* bus = BusOf(member);
        if (!member.open || frame.remote || bus == nullptr || bus->queue.full())
            return false;

        bus->queue.push_back(Pending{ member.participant, frame });
        if (!bus->onWire.has_value())
            StartNext(*bus);
        return true;
    }

    bool VirtualCanBus::ConflictsWithOpenMembers(const Member& member, uint32_t bitrate) const
    {
        return std::any_of(members.begin(), members.end(), [&member, bitrate](const Member& other)
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

    void VirtualCanBus::StartNext(Bus& bus)
    {
        if (bus.queue.empty())
            return;

        const auto next = std::min_element(bus.queue.begin(), bus.queue.end(), [](const Pending& left, const Pending& right)
            {
                return left.frame.id < right.frame.id;
            });
        bus.onWire = *next;
        bus.queue.erase(next);
        bus.timer.Start(TransmissionTime(bus.onWire->frame, bus.bitrate), [this, &bus]()
            {
                Deliver(bus);
            });
    }

    void VirtualCanBus::Deliver(Bus& bus)
    {
        const auto pending = *bus.onWire;
        bus.onWire.reset();
        const auto encoded = EncodeSlcanFrame(pending.frame.id, pending.frame.extended, pending.frame.data);

        for (auto& member : members)
            if (member.participant != nullptr && member.participant != pending.sender && member.open && member.bus == bus.name)
                member.participant->Write(encoded);

        StartNext(bus);
    }
}
