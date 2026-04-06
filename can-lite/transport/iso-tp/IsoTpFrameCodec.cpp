#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"

namespace services::iso_tp
{
    FrameType IsoTpFrameCodec::DecodeFrameType(const hal::Can::Message& frame)
    {
        return static_cast<FrameType>((frame[0] >> 4u) & 0x0Fu);
    }

    bool IsoTpFrameCodec::EncodeSingleFrame(infra::ConstByteRange pdu, hal::Can::Message& out)
    {
        if (pdu.size() == 0 || pdu.size() > sfMaxPayloadBytes)
            return false;

        out.clear();
        out.push_back(static_cast<uint8_t>(0x00u | (pdu.size() & 0x0Fu)));
        for (uint8_t byte : pdu)
            out.push_back(byte);
        return true;
    }

    uint8_t IsoTpFrameCodec::DecodeSingleFrameLength(const hal::Can::Message& frame)
    {
        return frame[0] & 0x0Fu;
    }

    void IsoTpFrameCodec::EncodeFirstFrame(infra::ConstByteRange pdu, hal::Can::Message& out)
    {
        auto len = static_cast<uint16_t>(pdu.size());
        out.clear();
        out.push_back(static_cast<uint8_t>(0x10u | ((len >> 8u) & 0x0Fu)));
        out.push_back(static_cast<uint8_t>(len & 0xFFu));
        for (std::size_t i = 0; i < ffFirstDataBytes && i < pdu.size(); ++i)
            out.push_back(pdu[i]);
    }

    uint16_t IsoTpFrameCodec::DecodeFirstFrameTotalLength(const hal::Can::Message& frame)
    {
        return static_cast<uint16_t>(((frame[0] & 0x0Fu) << 8u) | frame[1]);
    }

    uint8_t IsoTpFrameCodec::EncodeConsecutiveFrame(uint8_t sn, infra::ConstByteRange remaining, hal::Can::Message& out)
    {
        out.clear();
        out.push_back(static_cast<uint8_t>(0x20u | (sn & snMask)));
        uint8_t written = 0;
        for (std::size_t i = 0; i < cfMaxDataBytes && i < remaining.size(); ++i)
        {
            out.push_back(remaining[i]);
            ++written;
        }
        return written;
    }

    uint8_t IsoTpFrameCodec::DecodeConsecutiveFrameSn(const hal::Can::Message& frame)
    {
        return frame[0] & snMask;
    }

    void IsoTpFrameCodec::EncodeFlowControl(FlowStatus fs, uint8_t blockSize, uint8_t stMin, hal::Can::Message& out)
    {
        out.clear();
        out.push_back(static_cast<uint8_t>(0x30u | (static_cast<uint8_t>(fs) & 0x0Fu)));
        out.push_back(blockSize);
        out.push_back(stMin);
    }

    void IsoTpFrameCodec::DecodeFlowControl(const hal::Can::Message& frame, FlowStatus& fs, uint8_t& blockSize, uint8_t& stMin)
    {
        fs = static_cast<FlowStatus>(frame[0] & 0x0Fu);
        blockSize = frame[1];
        stMin = frame[2];
    }

    infra::Duration IsoTpFrameCodec::StMinToDuration(uint8_t stMin)
    {
        if (stMin <= 0x7Fu)
            return std::chrono::milliseconds(stMin);
        if (stMin >= 0xF1u && stMin <= 0xF9u)
            return std::chrono::microseconds((stMin - 0xF0u) * 100);
        return infra::Duration{};
    }
}
