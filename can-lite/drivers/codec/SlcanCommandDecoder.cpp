#include "can-lite/drivers/codec/SlcanCommandDecoder.hpp"

namespace services
{
    namespace
    {
        constexpr char carriageReturn{ '\r' };
        constexpr char joinPrefix{ '@' };

        SlcanCommandDecoder::Item Bitrate(infra::BoundedConstString line)
        {
            if (line.size() != 2)
                return {};

            const auto rate = SlcanBitrate(line[1]);
            if (!rate.has_value())
                return {};

            return { SlcanCommandDecoder::Command::bitrate, *rate, {}, {} };
        }

        SlcanCommandDecoder::Item Join(infra::BoundedConstString line)
        {
            const auto name = line.substr(1);
            if (name.empty() || name.size() > virtualBusMaxNameLength)
                return {};

            return { SlcanCommandDecoder::Command::join, 0, VirtualBusName{ name }, {} };
        }
    }

    SlcanCommand VirtualBusJoin(infra::BoundedConstString bus)
    {
        if (bus.empty() || bus.size() > virtualBusMaxNameLength)
            return {};

        SlcanCommand command;
        command.push_back(joinPrefix);
        command.append(bus);
        command.push_back(carriageReturn);
        return command;
    }

    std::optional<SlcanCommandDecoder::Item> SlcanCommandDecoder::Feed(char byte)
    {
        if (byte != carriageReturn)
        {
            if (line.full())
                discarding = true;
            else
                line.push_back(byte);
            return std::nullopt;
        }

        if (line.empty() && !discarding)
            return std::nullopt;

        auto item = discarding ? Item{} : ParseLine(line);
        Reset();
        return item;
    }

    void SlcanCommandDecoder::Reset()
    {
        line.clear();
        discarding = false;
    }

    SlcanCommandDecoder::Item SlcanCommandDecoder::ParseLine(infra::BoundedConstString line)
    {
        if (line == "C")
            return { Command::close, 0, {}, {} };
        if (line == "O")
            return { Command::open, 0, {}, {} };
        if (line.front() == 'S')
            return Bitrate(line);
        if (line.front() == joinPrefix)
            return Join(line);
        if (const auto frame = DecodeSlcanFrame(line))
            return { Command::frame, 0, {}, *frame };

        return {};
    }
}
