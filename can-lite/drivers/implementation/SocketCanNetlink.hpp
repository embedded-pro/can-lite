#pragma once

#ifdef __linux__

#include "infra/util/ByteRange.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services::socketcan
{
    struct LinkState
    {
        bool isCan{};
        bool up{};
        std::optional<uint32_t> bitrate;
    };

    std::size_t EncodeGetLink(infra::ByteRange buffer, int interfaceIndex, uint32_t sequence);
    std::size_t EncodeSetLinkUp(infra::ByteRange buffer, int interfaceIndex, bool up, uint32_t sequence);
    std::size_t EncodeSetBitrate(infra::ByteRange buffer, int interfaceIndex, uint32_t bitrate, uint32_t sequence);

    std::optional<LinkState> DecodeLink(infra::ConstByteRange response);
    std::optional<int> DecodeAcknowledgement(infra::ConstByteRange response);

    class RouteSocket
    {
    public:
        RouteSocket();
        RouteSocket(const RouteSocket&) = delete;
        RouteSocket& operator=(const RouteSocket&) = delete;
        ~RouteSocket();

        bool IsOpen() const;
        std::optional<LinkState> QueryLink(int interfaceIndex);
        int SetLinkUp(int interfaceIndex, bool up);
        int SetBitrate(int interfaceIndex, uint32_t bitrate);

    private:
        int Acknowledge(std::size_t requestSize);
        std::optional<infra::ConstByteRange> Exchange(std::size_t requestSize);

        int descriptor{ -1 };
        uint32_t sequence{};
        std::array<uint8_t, 256> request{};
        std::array<uint8_t, 8192> response{};
    };

    int ConfigureBitrate(int interfaceIndex, uint32_t bitrate);
}

#endif
