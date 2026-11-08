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

    TEST(CanNodeIdTest, DefaultBaseIsTheFirstIndividualNode)
    {
        EXPECT_EQ(canBroadcastNodeId + 1u, canDefaultNodeIdBase);
        EXPECT_EQ(MakeCanNodeId(canDefaultNodeIdBase, 0), 1);
    }

    TEST(CanNodeIdTest, OffsetIsAddedToTheBase)
    {
        EXPECT_EQ(MakeCanNodeId(1, 7), 8);
        EXPECT_EQ(MakeCanNodeId(10, 3), 13);
    }

    TEST(CanNodeIdTest, BroadcastBaseFallsBackToTheDefaultBase)
    {
        EXPECT_EQ(MakeCanNodeId(canBroadcastNodeId, 3), 4);
    }

    TEST(CanNodeIdTest, HighestNodeIsAccepted)
    {
        EXPECT_EQ(MakeCanNodeId(canIdNodeIdMask, 0), canIdNodeIdMask);
        EXPECT_EQ(MakeCanNodeId(canIdNodeIdMask - 7, 7), canIdNodeIdMask);
    }

    TEST(CanNodeIdTest, BaseThatOverflowsTheNodeFieldFallsBackToTheDefaultBase)
    {
        EXPECT_EQ(MakeCanNodeId(canIdNodeIdMask, 1), 2);
        EXPECT_EQ(MakeCanNodeId(canIdNodeIdMask + 1, 5), 6);
        EXPECT_EQ(MakeCanNodeId(UINT32_MAX, 0), 1);
    }

    static_assert(MakeCanNodeId(canDefaultNodeIdBase, 2) == 3);
}
