#pragma once

#include "can-lite/drivers/interface/CanBusAdapter.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services::gs_usb
{
    constexpr uint8_t requestHostFormat{};
    constexpr uint8_t requestBitTiming{ 1 };
    constexpr uint8_t requestMode{ 2 };
    constexpr uint8_t requestBitTimingConstants{ 4 };

    constexpr uint32_t hostFormat{ 0x0000beef };
    constexpr uint32_t modeReset{};
    constexpr uint32_t modeStart{ 1 };
    constexpr uint32_t receivedEchoId{ 0xffffffff };

    constexpr uint32_t extendedFlag{ 0x80000000 };
    constexpr uint32_t remoteFlag{ 0x40000000 };
    constexpr uint32_t errorFlag{ 0x20000000 };

    constexpr std::size_t frameSize{ 20 };
    constexpr std::size_t bitTimingConstantsSize{ 40 };
    constexpr std::size_t bitTimingSize{ 20 };
    constexpr std::size_t modeSize{ 8 };

    struct BitTimingConstants
    {
        uint32_t feature{};
        uint32_t clock{};
        uint32_t tseg1Min{};
        uint32_t tseg1Max{};
        uint32_t tseg2Min{};
        uint32_t tseg2Max{};
        uint32_t sjwMax{};
        uint32_t brpMin{};
        uint32_t brpMax{};
        uint32_t brpIncrement{};
    };

    struct BitTiming
    {
        uint32_t propSeg{};
        uint32_t phaseSeg1{};
        uint32_t phaseSeg2{};
        uint32_t sjw{};
        uint32_t brp{};
    };

    struct HostFrame
    {
        uint32_t echoId{ receivedEchoId };
        uint32_t id{};
        bool extended{ false };
        bool remote{ false };
        bool error{ false };
        uint8_t channel{};
        CanFrame data;
    };

    std::optional<BitTimingConstants> DecodeBitTimingConstants(const uint8_t* bytes, std::size_t size);
    std::optional<BitTiming> ComputeBitTiming(const BitTimingConstants& constants, uint32_t bitrate);

    std::array<uint8_t, 4> EncodeHostFormat();
    std::array<uint8_t, bitTimingSize> EncodeBitTiming(const BitTiming& timing);
    std::array<uint8_t, modeSize> EncodeMode(uint32_t mode);
    std::array<uint8_t, frameSize> EncodeFrame(uint32_t echoId, uint32_t id, bool extended, const CanFrame& data, uint8_t channel = 0);
    std::optional<HostFrame> DecodeFrame(const uint8_t* bytes, std::size_t size);
}
