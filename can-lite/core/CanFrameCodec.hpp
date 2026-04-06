#pragma once

#include "hal/interfaces/Can.hpp"
#include <cstdint>
#include <limits>

namespace services
{
    class CanFrameCodec
    {
    public:
        static int16_t FloatToFixed16(float value, int32_t scale);
        static float Fixed16ToFloat(int16_t value, int32_t scale);
        static int32_t FloatToFixed32(float value, int32_t scale);
        static float Fixed32ToFloat(int32_t value, int32_t scale);

        static void WriteInt16(hal::Can::Message& msg, std::size_t offset, int16_t value);
        static int16_t ReadInt16(const hal::Can::Message& msg, std::size_t offset);
        static void WriteInt32(hal::Can::Message& msg, std::size_t offset, int32_t value);
        static int32_t ReadInt32(const hal::Can::Message& msg, std::size_t offset);

        static void WriteUInt16(hal::Can::Message& msg, std::size_t offset, uint16_t value);
        static uint16_t ReadUInt16(const hal::Can::Message& msg, std::size_t offset);
        static void WriteUInt32(hal::Can::Message& msg, std::size_t offset, uint32_t value);
        static uint32_t ReadUInt32(const hal::Can::Message& msg, std::size_t offset);
    };
}
