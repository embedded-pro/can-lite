#include "can-lite/core/CanPayload.hpp"
#include "can-lite/core/CanFrameCodec.hpp"

namespace services
{
    CanPayloadWriter& CanPayloadWriter::WriteUInt8(uint8_t value)
    {
        if (Reserve(1))
            message.push_back(value);

        return *this;
    }

    CanPayloadWriter& CanPayloadWriter::WriteInt16(int16_t value)
    {
        if (Reserve(2))
            CanFrameCodec::WriteInt16(message, message.size(), value);

        return *this;
    }

    CanPayloadWriter& CanPayloadWriter::WriteUInt16(uint16_t value)
    {
        if (Reserve(2))
            CanFrameCodec::WriteUInt16(message, message.size(), value);

        return *this;
    }

    CanPayloadWriter& CanPayloadWriter::WriteInt32(int32_t value)
    {
        if (Reserve(4))
            CanFrameCodec::WriteInt32(message, message.size(), value);

        return *this;
    }

    CanPayloadWriter& CanPayloadWriter::WriteUInt32(uint32_t value)
    {
        if (Reserve(4))
            CanFrameCodec::WriteUInt32(message, message.size(), value);

        return *this;
    }

    CanPayloadWriter& CanPayloadWriter::WriteFixed16(float value, int32_t scale)
    {
        return WriteInt16(CanFrameCodec::FloatToFixed16(value, scale));
    }

    CanPayloadWriter& CanPayloadWriter::WriteBytes(infra::ConstByteRange bytes)
    {
        if (Reserve(bytes.size()))
            for (auto byte : bytes)
                message.push_back(byte);

        return *this;
    }

    bool CanPayloadWriter::Valid() const
    {
        return valid;
    }

    const hal::Can::Message& CanPayloadWriter::Message() const
    {
        return message;
    }

    bool CanPayloadWriter::Reserve(std::size_t bytes)
    {
        if (!valid)
            return false;

        if (message.size() + bytes > message.max_size())
        {
            valid = false;
            return false;
        }

        return true;
    }

    CanPayloadReader::CanPayloadReader(const hal::Can::Message& message)
        : message{ message }
    {}

    uint8_t CanPayloadReader::ReadUInt8()
    {
        auto start = Take(1);
        if (!start.has_value())
            return 0;

        return message[*start];
    }

    int16_t CanPayloadReader::ReadInt16()
    {
        auto start = Take(2);
        if (!start.has_value())
            return 0;

        return CanFrameCodec::ReadInt16(message, *start);
    }

    uint16_t CanPayloadReader::ReadUInt16()
    {
        auto start = Take(2);
        if (!start.has_value())
            return 0;

        return CanFrameCodec::ReadUInt16(message, *start);
    }

    int32_t CanPayloadReader::ReadInt32()
    {
        auto start = Take(4);
        if (!start.has_value())
            return 0;

        return CanFrameCodec::ReadInt32(message, *start);
    }

    uint32_t CanPayloadReader::ReadUInt32()
    {
        auto start = Take(4);
        if (!start.has_value())
            return 0;

        return CanFrameCodec::ReadUInt32(message, *start);
    }

    float CanPayloadReader::ReadFixed16(int32_t scale)
    {
        return CanFrameCodec::Fixed16ToFloat(ReadInt16(), scale);
    }

    void CanPayloadReader::Skip(std::size_t bytes)
    {
        Take(bytes);
    }

    infra::ConstByteRange CanPayloadReader::ReadRemaining()
    {
        if (!valid)
            return infra::ConstByteRange{};

        auto remaining = infra::ConstByteRange{ message.begin() + offset, message.end() };
        offset = message.size();
        return remaining;
    }

    bool CanPayloadReader::Valid() const
    {
        return valid;
    }

    std::size_t CanPayloadReader::Available() const
    {
        return valid ? message.size() - offset : 0;
    }

    std::optional<std::size_t> CanPayloadReader::Take(std::size_t bytes)
    {
        if (!valid)
            return std::nullopt;

        if (offset + bytes > message.size())
        {
            valid = false;
            return std::nullopt;
        }

        auto start = offset;
        offset += bytes;
        return start;
    }
}
