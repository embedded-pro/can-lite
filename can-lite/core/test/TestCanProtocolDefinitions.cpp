#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace services;

    TEST(CanIdTest, RoundTrip)
    {
        uint32_t id = MakeCanId(CanPriority::command, 0x01, 0x02, 42);

        EXPECT_EQ(ExtractCanPriority(id), CanPriority::command);
        EXPECT_EQ(ExtractCanCategory(id), 0x01);
        EXPECT_EQ(ExtractCanMessageType(id), 0x02);
        EXPECT_EQ(ExtractCanNodeId(id), 42);
    }

    TEST(CanIdTest, AllPriorities)
    {
        for (auto p : { CanPriority::emergency, CanPriority::command, CanPriority::telemetry, CanPriority::heartbeat })
        {
            uint32_t id = MakeCanId(p, 0x00, 0x01, 1);
            EXPECT_EQ(ExtractCanPriority(id), p);
        }
    }

    TEST(CanIdTest, AllCategoryValues)
    {
        for (uint8_t c = 0; c < 16; ++c)
        {
            uint32_t id = MakeCanId(CanPriority::command, c, 0x01, 1);
            EXPECT_EQ(ExtractCanCategory(id), c);
        }
    }

    TEST(CanIdTest, BroadcastNodeId)
    {
        uint32_t id = MakeCanId(CanPriority::emergency, 0x00, 0x03, canBroadcastNodeId);
        EXPECT_EQ(ExtractCanNodeId(id), canBroadcastNodeId);
    }

    TEST(CanIdTest, MaxNodeId)
    {
        uint32_t id = MakeCanId(CanPriority::command, 0x00, 0x01, 0xFFF);
        EXPECT_EQ(ExtractCanNodeId(id), 0xFFF);
    }

    TEST(CanIdTest, MessageTypeRange)
    {
        for (uint8_t mt : { 0x00, 0x01, 0x7F, 0xFF })
        {
            uint32_t id = MakeCanId(CanPriority::command, 0x00, mt, 1);
            EXPECT_EQ(ExtractCanMessageType(id), mt);
        }
    }
}
