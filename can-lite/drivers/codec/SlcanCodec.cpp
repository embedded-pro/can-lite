#include "can-lite/drivers/codec/SlcanCodec.hpp"
#include <array>
#include <utility>

namespace services
{
    namespace
    {
        constexpr char bell{ '\a' };
        constexpr char carriageReturn{ '\r' };
        constexpr std::size_t standardIdDigits{ 3 };
        constexpr std::size_t extendedIdDigits{ 8 };
        constexpr uint32_t standardIdMask{ 0x7ff };
        constexpr uint32_t extendedIdMask{ 0x1fffffff };

        constexpr std::array<std::pair<uint32_t, char>, 9> bitrateCodes{ {
            { 10000, '0' },
            { 20000, '1' },
            { 50000, '2' },
            { 100000, '3' },
            { 125000, '4' },
            { 250000, '5' },
            { 500000, '6' },
            { 800000, '7' },
            { 1000000, '8' },
        } };

        std::optional<uint8_t> HexDigit(char character)
        {
            if (character >= '0' && character <= '9')
                return static_cast<uint8_t>(character - '0');
            if (character >= 'a' && character <= 'f')
                return static_cast<uint8_t>(character - 'a' + 10);
            if (character >= 'A' && character <= 'F')
                return static_cast<uint8_t>(character - 'A' + 10);
            return std::nullopt;
        }

        std::optional<uint32_t> ParseHex(infra::BoundedConstString text, std::size_t offset, std::size_t digits)
        {
            if (text.size() < offset + digits)
                return std::nullopt;

            uint32_t value{};
            for (std::size_t index = 0; index < digits; ++index)
            {
                const auto digit = HexDigit(text[offset + index]);
                if (!digit)
                    return std::nullopt;
                value = (value << 4) | *digit;
            }
            return value;
        }

        void AppendHex(infra::BoundedString& out, uint32_t value, std::size_t digits)
        {
            static constexpr std::array<char, 16> hexDigits{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
            for (std::size_t index = digits; index > 0; --index)
                out.push_back(hexDigits[(value >> (4 * (index - 1))) & 0xf]);
        }

        bool IsFrameType(char type)
        {
            return type == 't' || type == 'T' || type == 'r' || type == 'R';
        }

        bool ParsePayload(infra::BoundedConstString line, std::size_t offset, std::size_t length, CanFrame& data)
        {
            for (std::size_t index = 0; index < length; ++index)
            {
                const auto byte = ParseHex(line, offset + 2 * index, 2);
                if (!byte)
                    return false;
                data.push_back(static_cast<uint8_t>(*byte));
            }
            return true;
        }

        std::optional<SlcanFrame> ParseFrame(infra::BoundedConstString line)
        {
            const auto type = line.front();
            const bool extended{ type == 'T' || type == 'R' };
            const bool remote{ type == 'r' || type == 'R' };
            const auto idDigits = extended ? extendedIdDigits : standardIdDigits;

            const auto id = ParseHex(line, 1, idDigits);
            const auto length = line.size() > 1 + idDigits ? HexDigit(line[1 + idDigits]) : std::nullopt;
            if (!id || !length || *length > 8 || *id > (extended ? extendedIdMask : standardIdMask))
                return std::nullopt;

            SlcanFrame frame{ *id, extended, remote, {} };
            if (!remote && !ParsePayload(line, 2 + idDigits, *length, frame.data))
                return std::nullopt;

            return frame;
        }
    }

    std::optional<char> SlcanBitrateCode(uint32_t bitrate)
    {
        for (const auto& [rate, code] : bitrateCodes)
            if (rate == bitrate)
                return code;
        return std::nullopt;
    }

    SlcanCommand SlcanOpenCommands(char bitrateCode)
    {
        SlcanCommand command{ "C\rS" };
        command.push_back(bitrateCode);
        command.append("\rO\r");
        return command;
    }

    SlcanCommand SlcanCloseCommand()
    {
        return SlcanCommand{ "C\r" };
    }

    SlcanCommand EncodeSlcanFrame(uint32_t id, bool extended, const CanFrame& data)
    {
        SlcanCommand command;
        command.push_back(extended ? 'T' : 't');
        AppendHex(command, id & (extended ? extendedIdMask : standardIdMask), extended ? extendedIdDigits : standardIdDigits);
        command.push_back(static_cast<char>('0' + data.size()));
        for (const auto byte : data)
            AppendHex(command, byte, 2);
        command.push_back(carriageReturn);
        return command;
    }

    std::optional<SlcanDecoder::Item> SlcanDecoder::Feed(char byte)
    {
        if (byte == bell)
        {
            Reset();
            return Item{ Event::error, {} };
        }

        if (byte != carriageReturn)
        {
            if (line.full())
                discarding = true;
            else
                line.push_back(byte);
            return std::nullopt;
        }

        auto item = discarding ? std::nullopt : ParseLine(line);
        Reset();
        return item;
    }

    void SlcanDecoder::Reset()
    {
        line.clear();
        discarding = false;
    }

    std::optional<SlcanDecoder::Item> SlcanDecoder::ParseLine(infra::BoundedConstString line)
    {
        if (line.empty() || line == "z" || line == "Z")
            return Item{ Event::acknowledged, {} };

        if (IsFrameType(line.front()))
            if (const auto frame = ParseFrame(line))
                return Item{ Event::frame, *frame };

        return std::nullopt;
    }
}
