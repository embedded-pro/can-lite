#pragma once

#include "can-lite/core/CanProtocolDefinitions.hpp"
#include <cstdint>

namespace services
{
    static constexpr uint8_t setpointCategoryId = 0x02;

    static constexpr uint8_t setpointSetId = 0x00;
    static constexpr uint8_t setpointRequestStatusId = 0x01;

    static constexpr uint8_t setpointStatusId = 0x81;

    static constexpr int32_t setpointValueScale = 100;

    static_assert(IsApplicationCategoryId(setpointCategoryId));
    static_assert(IsCommandMessageType(setpointSetId));
    static_assert(IsCommandMessageType(setpointRequestStatusId));
    static_assert(IsResponseMessageType(setpointStatusId));

    struct SetpointStatus
    {
        float setpoint;
        float measurement;
    };
}
