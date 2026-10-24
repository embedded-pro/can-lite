#include "can-lite/drivers/codec/GsUsbProtocol.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace services::gs_usb
{
    namespace
    {
        constexpr double targetSamplePoint{ 0.875 };
        constexpr uint32_t standardIdMask{ 0x7ff };
        constexpr uint32_t extendedIdMask{ 0x1fffffff };

        uint32_t ReadLittleEndian(const uint8_t* bytes)
        {
            return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) | (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
        }

        template<std::size_t Size>
        void WriteLittleEndian(std::array<uint8_t, Size>& bytes, std::size_t offset, uint32_t value)
        {
            for (std::size_t index = 0; index < 4; ++index)
                bytes[offset + index] = static_cast<uint8_t>(value >> (8 * index));
        }

        struct Candidate
        {
            BitTiming timing;
            double samplePointError{ 1.0 };
        };

        std::optional<Candidate> CandidateFor(const BitTimingConstants& constants, uint32_t brp, uint32_t bitrate)
        {
            const auto divisor = static_cast<uint64_t>(brp) * bitrate;
            if (divisor == 0 || constants.clock % divisor != 0)
                return std::nullopt;

            const auto quanta = static_cast<uint32_t>(constants.clock / divisor);
            if (quanta < 1 + constants.tseg1Min + constants.tseg2Min || quanta > 1 + constants.tseg1Max + constants.tseg2Max)
                return std::nullopt;

            auto tseg2 = static_cast<uint32_t>(std::lround(quanta * (1.0 - targetSamplePoint)));
            tseg2 = std::clamp(tseg2, constants.tseg2Min, constants.tseg2Max);
            auto tseg1 = quanta - 1 - tseg2;
            if (tseg1 > constants.tseg1Max)
            {
                tseg1 = constants.tseg1Max;
                tseg2 = quanta - 1 - tseg1;
            }
            else if (tseg1 < constants.tseg1Min)
            {
                tseg1 = constants.tseg1Min;
                tseg2 = quanta - 1 - tseg1;
            }

            if (tseg2 < constants.tseg2Min || tseg2 > constants.tseg2Max)
                return std::nullopt;

            const auto propSeg = tseg1 / 2;
            const auto samplePoint = static_cast<double>(1 + tseg1) / quanta;
            return Candidate{ BitTiming{ propSeg, tseg1 - propSeg, tseg2, std::min<uint32_t>(constants.sjwMax, 1), brp }, std::abs(samplePoint - targetSamplePoint) };
        }
    }

    std::optional<BitTimingConstants> DecodeBitTimingConstants(const uint8_t* bytes, std::size_t size)
    {
        if (size < bitTimingConstantsSize)
            return std::nullopt;

        BitTimingConstants constants;
        const std::array<uint32_t*, 10> fields{ &constants.feature, &constants.clock, &constants.tseg1Min, &constants.tseg1Max, &constants.tseg2Min, &constants.tseg2Max, &constants.sjwMax, &constants.brpMin, &constants.brpMax, &constants.brpIncrement };
        for (std::size_t index = 0; index < fields.size(); ++index)
            *fields[index] = ReadLittleEndian(bytes + 4 * index);

        if (constants.clock == 0 || constants.brpMin == 0 || constants.brpMax < constants.brpMin || constants.tseg1Max < constants.tseg1Min || constants.tseg2Max < constants.tseg2Min)
            return std::nullopt;

        return constants;
    }

    std::optional<BitTiming> ComputeBitTiming(const BitTimingConstants& constants, uint32_t bitrate)
    {
        std::optional<Candidate> best;
        const auto step = std::max<uint32_t>(constants.brpIncrement, 1);
        for (auto brp = constants.brpMin; brp <= constants.brpMax; brp += step)
        {
            const auto candidate = CandidateFor(constants, brp, bitrate);
            if (candidate && (!best || candidate->samplePointError < best->samplePointError))
                best = candidate;
        }

        if (!best)
            return std::nullopt;
        return best->timing;
    }

    std::array<uint8_t, 4> EncodeHostFormat()
    {
        std::array<uint8_t, 4> bytes{};
        WriteLittleEndian(bytes, 0, hostFormat);
        return bytes;
    }

    std::array<uint8_t, bitTimingSize> EncodeBitTiming(const BitTiming& timing)
    {
        std::array<uint8_t, bitTimingSize> bytes{};
        WriteLittleEndian(bytes, 0, timing.propSeg);
        WriteLittleEndian(bytes, 4, timing.phaseSeg1);
        WriteLittleEndian(bytes, 8, timing.phaseSeg2);
        WriteLittleEndian(bytes, 12, timing.sjw);
        WriteLittleEndian(bytes, 16, timing.brp);
        return bytes;
    }

    std::array<uint8_t, modeSize> EncodeMode(uint32_t mode)
    {
        std::array<uint8_t, modeSize> bytes{};
        WriteLittleEndian(bytes, 0, mode);
        return bytes;
    }

    std::array<uint8_t, frameSize> EncodeFrame(uint32_t echoId, uint32_t id, bool extended, const CanFrame& data, uint8_t channel)
    {
        std::array<uint8_t, frameSize> bytes{};
        WriteLittleEndian(bytes, 0, echoId);
        WriteLittleEndian(bytes, 4, extended ? ((id & extendedIdMask) | extendedFlag) : (id & standardIdMask));
        bytes[8] = static_cast<uint8_t>(data.size());
        bytes[9] = channel;
        std::copy(data.begin(), data.end(), bytes.begin() + 12);
        return bytes;
    }

    std::optional<HostFrame> DecodeFrame(const uint8_t* bytes, std::size_t size)
    {
        if (size < frameSize)
            return std::nullopt;

        const auto rawId = ReadLittleEndian(bytes + 4);
        const auto length = bytes[8];
        if (length > 8)
            return std::nullopt;

        HostFrame frame;
        frame.echoId = ReadLittleEndian(bytes);
        frame.extended = (rawId & extendedFlag) != 0;
        frame.remote = (rawId & remoteFlag) != 0;
        frame.error = (rawId & errorFlag) != 0;
        frame.id = rawId & (frame.extended ? extendedIdMask : standardIdMask);
        frame.channel = bytes[9];
        if (!frame.remote)
            frame.data.assign(bytes + 12, bytes + 12 + length);
        return frame;
    }
}
