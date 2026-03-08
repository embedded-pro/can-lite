#pragma once

#include <cstdint>

namespace services
{
    static constexpr uint8_t canProtocolVersion = 1;

    static constexpr uint32_t canIdPriorityShift = 24;
    static constexpr uint32_t canIdCategoryShift = 20;
    static constexpr uint32_t canIdMessageTypeShift = 12;
    static constexpr uint32_t canIdNodeIdMask = 0xFFF;
    static constexpr uint32_t canBroadcastNodeId = 0x000;

    static constexpr uint8_t canSystemCategoryId = 0x00;
    static constexpr uint8_t canHeartbeatMessageTypeId = 0x01;
    static constexpr uint8_t canCommandAckMessageTypeId = 0x02;
    static constexpr uint8_t canStatusRequestMessageTypeId = 0x03;
    static constexpr uint8_t canCategoryListRequestMessageTypeId = 0x04;
    static constexpr uint8_t canCategoryListResponseMessageTypeId = 0x05;

    enum class CanPriority : uint8_t
    {
        emergency = 0,
        command = 4,
        response = 8,
        telemetry = 12,
        heartbeat = 16
    };

    enum class CanAckStatus : uint8_t
    {
        success = 0,
        unknownCommand = 1,
        invalidPayload = 2,
        invalidState = 3,
        sequenceError = 4,
        rateLimited = 5
    };

    static constexpr uint32_t MakeCanId(CanPriority priority, uint8_t category,
        uint8_t messageType, uint16_t nodeId)
    {
        return (static_cast<uint32_t>(priority) << canIdPriorityShift) |
               (static_cast<uint32_t>(category) << canIdCategoryShift) |
               (static_cast<uint32_t>(messageType) << canIdMessageTypeShift) |
               (static_cast<uint32_t>(nodeId) & canIdNodeIdMask);
    }

    static constexpr CanPriority ExtractCanPriority(uint32_t canId)
    {
        return static_cast<CanPriority>((canId >> canIdPriorityShift) & 0x1F);
    }

    static constexpr uint8_t ExtractCanCategory(uint32_t canId)
    {
        return static_cast<uint8_t>((canId >> canIdCategoryShift) & 0xF);
    }

    static constexpr uint8_t ExtractCanMessageType(uint32_t canId)
    {
        return static_cast<uint8_t>((canId >> canIdMessageTypeShift) & 0xFF);
    }

    static constexpr uint16_t ExtractCanNodeId(uint32_t canId)
    {
        return static_cast<uint16_t>(canId & canIdNodeIdMask);
    }
}
