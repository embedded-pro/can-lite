#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    class CanPayloadWriter
    {
    public:
        CanPayloadWriter() = default;

        CanPayloadWriter& WriteUInt8(uint8_t value);
        CanPayloadWriter& WriteInt16(int16_t value);
        CanPayloadWriter& WriteUInt16(uint16_t value);
        CanPayloadWriter& WriteInt32(int32_t value);
        CanPayloadWriter& WriteUInt32(uint32_t value);
        CanPayloadWriter& WriteFixed16(float value, int32_t scale);
        CanPayloadWriter& WriteBytes(infra::ConstByteRange bytes);

        bool Valid() const;
        const hal::Can::Message& Message() const;

    private:
        bool Reserve(std::size_t bytes);

        hal::Can::Message message;
        bool valid{ true };
    };

    class CanPayloadReader
    {
    public:
        explicit CanPayloadReader(const hal::Can::Message& message);

        uint8_t ReadUInt8();
        int16_t ReadInt16();
        uint16_t ReadUInt16();
        int32_t ReadInt32();
        uint32_t ReadUInt32();
        float ReadFixed16(int32_t scale);

        void Skip(std::size_t bytes);
        infra::ConstByteRange ReadRemaining();

        bool Valid() const;
        std::size_t Available() const;

    private:
        std::optional<std::size_t> Take(std::size_t bytes);

        const hal::Can::Message& message;
        std::size_t offset{};
        bool valid{ true };
    };
}
