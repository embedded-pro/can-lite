#pragma once

#include "infra/timer/Timer.hpp"
#include <cstdint>

namespace services::iso_tp
{
    enum class FrameType : uint8_t
    {
        singleFrame = 0,
        firstFrame = 1,
        consecutiveFrame = 2,
        flowControl = 3
    };

    enum class FlowStatus : uint8_t
    {
        continueToSend = 0,
        wait = 1,
        overflow = 2
    };

    enum class SenderState : uint8_t
    {
        idle,
        waitingForFc,
        sendingCf
    };

    enum class ReceiverState : uint8_t
    {
        idle,
        receivingCf
    };

    enum class AbortReason : uint8_t
    {
        nBsTimeout,
        nCrTimeout,
        overflow,
        unexpectedFrame
    };

    static constexpr infra::Duration nBsTimeout = std::chrono::milliseconds(1000);
    static constexpr infra::Duration nCrTimeout = std::chrono::milliseconds(1000);

    static constexpr uint8_t sfMaxPayloadBytes = 7u;
    static constexpr uint8_t ffFirstDataOffset = 2u;
    static constexpr uint8_t ffFirstDataBytes = 6u;
    static constexpr uint8_t cfMaxDataBytes = 7u;
    static constexpr uint8_t snMask = 0x0Fu;
}
