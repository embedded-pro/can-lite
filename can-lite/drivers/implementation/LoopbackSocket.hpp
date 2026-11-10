#pragma once

#include "infra/util/ByteRange.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

#if defined(__linux__) || defined(_WIN32)

namespace services::loopback
{
    constexpr uint16_t virtualCanHubPort{ 29536 };
    constexpr intptr_t invalidSocket{ -1 };

    intptr_t Connect(uint16_t port);
    intptr_t Listen(uint16_t port);
    std::optional<uint16_t> LocalPort(intptr_t socket);
    intptr_t Accept(intptr_t listener);
    bool Send(intptr_t socket, infra::ConstByteRange bytes);
    std::optional<std::size_t> Receive(intptr_t socket, infra::ByteRange buffer);
    void Close(intptr_t socket);
}

#endif
