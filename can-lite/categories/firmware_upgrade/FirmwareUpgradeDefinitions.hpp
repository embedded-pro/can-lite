#pragma once

#include <cstdint>

namespace services
{
    static constexpr uint8_t firmwareUpgradeCategoryId = 0x01;

    // Command message type IDs (Client → Server) 0x00–0x7F
    static constexpr uint8_t fwuBeginUpgradeId = 0x00;
    static constexpr uint8_t fwuDataBlockId = 0x01;
    static constexpr uint8_t fwuVerifyId = 0x02;
    static constexpr uint8_t fwuActivateId = 0x03;
    static constexpr uint8_t fwuAbortId = 0x04;
    static constexpr uint8_t fwuQueryProgressId = 0x05;

    // Response message type IDs (Server → Client) 0x80+
    static constexpr uint8_t fwuBeginResponseId = 0x80;
    static constexpr uint8_t fwuDataBlockAckId = 0x81;
    static constexpr uint8_t fwuVerifyResponseId = 0x82;
    static constexpr uint8_t fwuActivateResponseId = 0x83;
    static constexpr uint8_t fwuProgressResponseId = 0x85;

    enum class FwuState : uint8_t
    {
        idle = 0,
        receiving = 1,
        verifying = 2,
        complete = 3,
        error = 4
    };

    enum class FwuError : uint8_t
    {
        ok = 0,
        busy = 1,
        invalidSize = 2,
        sequenceError = 3,
        writeError = 4,
        crcMismatch = 5,
        notReady = 6,
        invalidState = 7,
        sessionTimeout = 8
    };
}
