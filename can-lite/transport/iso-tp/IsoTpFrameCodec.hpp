#pragma once

#include "can-lite/transport/iso-tp/IsoTpTypes.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include <cstdint>

namespace services::iso_tp
{
    class IsoTpFrameCodec
    {
    public:
        static FrameType DecodeFrameType(const hal::Can::Message& frame);

        static bool EncodeSingleFrame(infra::ConstByteRange pdu, hal::Can::Message& out);
        static uint8_t DecodeSingleFrameLength(const hal::Can::Message& frame);

        static void EncodeFirstFrame(infra::ConstByteRange pdu, hal::Can::Message& out);
        static uint16_t DecodeFirstFrameTotalLength(const hal::Can::Message& frame);

        static uint8_t EncodeConsecutiveFrame(uint8_t sn, infra::ConstByteRange remaining, hal::Can::Message& out);
        static uint8_t DecodeConsecutiveFrameSn(const hal::Can::Message& frame);

        static void EncodeFlowControl(FlowStatus fs, uint8_t blockSize, uint8_t stMin, hal::Can::Message& out);
        static void DecodeFlowControl(const hal::Can::Message& frame, FlowStatus& fs, uint8_t& blockSize, uint8_t& stMin);

        static infra::Duration StMinToDuration(uint8_t stMin);
    };
}
