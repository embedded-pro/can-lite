#include "can-lite/core/CanFrameCodec.hpp"
#include "gtest/gtest.h"
#include <limits>

namespace
{
    using namespace services;

    TEST(CanFrameCodecTest, FloatToFixed16_NormalValue)
    {
        auto result = CanFrameCodec::FloatToFixed16(1.5f, 1000);
        EXPECT_EQ(result, 1500);
    }

    TEST(CanFrameCodecTest, FloatToFixed16_NegativeValue)
    {
        auto result = CanFrameCodec::FloatToFixed16(-2.5f, 1000);
        EXPECT_EQ(result, -2500);
    }

    TEST(CanFrameCodecTest, FloatToFixed16_SaturatesAtMax)
    {
        auto result = CanFrameCodec::FloatToFixed16(100.0f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int16_t>::max());
    }

    TEST(CanFrameCodecTest, FloatToFixed16_SaturatesAtMin)
    {
        auto result = CanFrameCodec::FloatToFixed16(-100.0f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int16_t>::min());
    }

    TEST(CanFrameCodecTest, FloatToFixed16_RoundsToNearest)
    {
        auto result = CanFrameCodec::FloatToFixed16(1.2345f, 1000);
        EXPECT_EQ(result, 1235);
    }

    TEST(CanFrameCodecTest, Fixed16ToFloat_RoundTrip)
    {
        int16_t fixed = CanFrameCodec::FloatToFixed16(3.14f, 1000);
        float result = CanFrameCodec::Fixed16ToFloat(fixed, 1000);
        EXPECT_NEAR(result, 3.14f, 0.01f);
    }

    TEST(CanFrameCodecTest, FloatToFixed32_NormalValue)
    {
        auto result = CanFrameCodec::FloatToFixed32(1.5f, 1000);
        EXPECT_EQ(result, 1500);
    }

    TEST(CanFrameCodecTest, FloatToFixed32_SaturatesAtMax)
    {
        auto result = CanFrameCodec::FloatToFixed32(1e15f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int32_t>::max());
    }

    TEST(CanFrameCodecTest, FloatToFixed32_SaturatesAtMin)
    {
        auto result = CanFrameCodec::FloatToFixed32(-1e15f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int32_t>::min());
    }

    TEST(CanFrameCodecTest, FloatToFixed32_RoundsToNearest)
    {
        auto result = CanFrameCodec::FloatToFixed32(1.2345f, 1000);
        EXPECT_EQ(result, 1235);
    }

    TEST(CanFrameCodecTest, FloatToFixed32_NanReturnsZero)
    {
        auto result = CanFrameCodec::FloatToFixed32(std::numeric_limits<float>::quiet_NaN(), 1000);
        EXPECT_EQ(result, 0);
    }

    TEST(CanFrameCodecTest, FloatToFixed32_ExactlyAtInt32MaxBoundary)
    {
        auto result = CanFrameCodec::FloatToFixed32(static_cast<float>(std::numeric_limits<int32_t>::max()), 1);
        EXPECT_EQ(result, std::numeric_limits<int32_t>::max());
    }

    TEST(CanFrameCodecTest, Fixed32ToFloat_RoundTrip)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(3.14f, 10000);
        float result = CanFrameCodec::Fixed32ToFloat(fixed, 10000);
        EXPECT_NEAR(result, 3.14f, 0.001f);
    }

    TEST(CanFrameCodecTest, FloatToFixed16_NanReturnsZero)
    {
        auto result = CanFrameCodec::FloatToFixed16(std::numeric_limits<float>::quiet_NaN(), 1000);
        EXPECT_EQ(result, 0);
    }

    TEST(CanFrameCodecTest, ZeroScaleReturnsZeroInsteadOfDividingByIt)
    {
        EXPECT_EQ(CanFrameCodec::FloatToFixed16(1.5f, 0), 0);
        EXPECT_EQ(CanFrameCodec::FloatToFixed32(1.5f, 0), 0);
        EXPECT_EQ(CanFrameCodec::Fixed16ToFloat(1234, 0), 0.0f);
        EXPECT_EQ(CanFrameCodec::Fixed32ToFloat(1234, 0), 0.0f);
    }

    TEST(CanFrameCodecTest, WritePastFrameCapacity_FailsInsteadOfOverflowing)
    {
        hal::Can::Message msg;

        EXPECT_FALSE(CanFrameCodec::WriteInt16(msg, 7, 0x1234));
        EXPECT_FALSE(CanFrameCodec::WriteUInt16(msg, 7, 0x1234));
        EXPECT_FALSE(CanFrameCodec::WriteInt32(msg, 5, 0x12345678));
        EXPECT_FALSE(CanFrameCodec::WriteUInt32(msg, 5, 0x12345678));
        EXPECT_LE(msg.size(), msg.max_size());

        EXPECT_TRUE(CanFrameCodec::WriteUInt32(msg, 4, 0x12345678));
        EXPECT_EQ(msg.size(), 8u);
    }

    TEST(CanFrameCodecTest, WriteAtOverflowingOffset_FailsInsteadOfWrapping)
    {
        hal::Can::Message msg;

        EXPECT_FALSE(CanFrameCodec::WriteUInt16(msg, std::numeric_limits<std::size_t>::max(), 0x1234));
        EXPECT_FALSE(CanFrameCodec::WriteInt16(msg, std::numeric_limits<std::size_t>::max() - 1u, 0x1234));
        EXPECT_FALSE(CanFrameCodec::WriteUInt32(msg, std::numeric_limits<std::size_t>::max() - 3u, 0x12345678));
        EXPECT_FALSE(CanFrameCodec::WriteInt32(msg, std::numeric_limits<std::size_t>::max(), 0x12345678));

        EXPECT_TRUE(msg.empty());
    }

    TEST(CanFrameCodecTest, ReadAtOverflowingOffset_ReturnsZeroInsteadOfWrapping)
    {
        hal::Can::Message msg;
        for (uint8_t i = 0; i != 8u; ++i)
            msg.push_back(i);

        EXPECT_EQ(CanFrameCodec::ReadUInt16(msg, std::numeric_limits<std::size_t>::max()), 0u);
        EXPECT_EQ(CanFrameCodec::ReadInt16(msg, std::numeric_limits<std::size_t>::max() - 1u), 0);
        EXPECT_EQ(CanFrameCodec::ReadUInt32(msg, std::numeric_limits<std::size_t>::max() - 3u), 0u);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, std::numeric_limits<std::size_t>::max()), 0);
    }

    TEST(CanFrameCodecTest, ReadPastFrameEnd_ReturnsZeroInsteadOfIndexingOutOfRange)
    {
        hal::Can::Message msg;
        msg.push_back(0xAA);

        EXPECT_EQ(CanFrameCodec::ReadInt16(msg, 0), 0);
        EXPECT_EQ(CanFrameCodec::ReadUInt16(msg, 0), 0u);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, 0), 0);
        EXPECT_EQ(CanFrameCodec::ReadUInt32(msg, 0), 0u);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt16_RoundTrip)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, 12345);
        EXPECT_EQ(CanFrameCodec::ReadInt16(msg, 0), 12345);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt16_NegativeValue)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, -9876);
        EXPECT_EQ(CanFrameCodec::ReadInt16(msg, 0), -9876);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt32_RoundTrip)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt32(msg, 0, 123456);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, 0), 123456);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt32_NegativeValue)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt32(msg, 0, -987654);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, 0), -987654);
    }

    TEST(CanFrameCodecTest, WriteInt16_AtOffset)
    {
        hal::Can::Message msg;
        msg.push_back(0xAA);
        CanFrameCodec::WriteInt16(msg, 1, 256);
        EXPECT_EQ(msg[0], 0xAA);
        EXPECT_EQ(CanFrameCodec::ReadInt16(msg, 1), 256);
    }

    TEST(CanFrameCodecTest, WriteInt32_AtOffset)
    {
        hal::Can::Message msg;
        msg.push_back(0xBB);
        CanFrameCodec::WriteInt32(msg, 1, 65536);
        EXPECT_EQ(msg[0], 0xBB);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, 1), 65536);
    }
}
