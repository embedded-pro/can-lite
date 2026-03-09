#pragma once

#include "hal/interfaces/Can.hpp"
#include <cstdint>

namespace integration
{
    struct CanIdResult
    {
        uint32_t rawId = 0;
    };

    struct CategoryDiscoveryResult
    {
        hal::Can::Message categories;
        bool received = false;
    };
}
