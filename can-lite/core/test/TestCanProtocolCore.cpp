#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanMessageType.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "gtest/gtest.h"
#include <limits>

namespace
{
    using namespace services;

    // --- CanMessageType ---

    class StubMessageType : public CanMessageType
    {
    public:
        explicit StubMessageType(uint8_t id)
            : id(id)
        {}

        uint8_t Id() const override
        {
            return id;
        }

        void Handle(const hal::Can::Message& data) override
        {
            lastDataSize = data.size();
            handleCallCount++;
        }

        std::size_t lastDataSize = 0;
        int handleCallCount = 0;

    private:
        uint8_t id;
    };

    // --- CanCategory ---

    class StubCategory : public CanCategory
    {
    public:
        explicit StubCategory(uint8_t id)
            : id(id)
        {}

        uint8_t Id() const override
        {
            return id;
        }

    private:
        uint8_t id;
    };

    class NoSequenceCategory : public CanCategory
    {
    public:
        uint8_t Id() const override
        {
            return 0x0F;
        }

        bool RequiresSequenceValidation() const override
        {
            return false;
        }
    };

    TEST(CanCategoryTest, DefaultRequiresSequenceValidation)
    {
        StubCategory category(0x01);
        EXPECT_TRUE(category.RequiresSequenceValidation());
    }

    TEST(CanCategoryTest, OverriddenSequenceValidation)
    {
        NoSequenceCategory category;
        EXPECT_FALSE(category.RequiresSequenceValidation());
    }

    TEST(CanCategoryTest, IdReturnsConfiguredValue)
    {
        StubCategory cat1(0x01);
        StubCategory cat2(0x05);
        StubCategory cat3(0x0F);

        EXPECT_EQ(cat1.Id(), 0x01);
        EXPECT_EQ(cat2.Id(), 0x05);
        EXPECT_EQ(cat3.Id(), 0x0F);
    }

    TEST(CanCategoryTest, HandleMessageDispatchesToRegisteredMessageType)
    {
        StubCategory category(0x01);
        StubMessageType msg1(0x01);
        StubMessageType msg2(0x02);
        category.AddMessageType(msg1);
        category.AddMessageType(msg2);

        hal::Can::Message data;
        data.push_back(0xAA);
        data.push_back(0xBB);

        EXPECT_TRUE(category.HandleMessage(0x01, data));
        EXPECT_EQ(msg1.handleCallCount, 1);
        EXPECT_EQ(msg1.lastDataSize, 2u);
        EXPECT_EQ(msg2.handleCallCount, 0);
    }

    TEST(CanCategoryTest, HandleMessageReturnsFalseForUnknownType)
    {
        StubCategory category(0x01);
        StubMessageType msg1(0x01);
        category.AddMessageType(msg1);

        hal::Can::Message data;
        EXPECT_FALSE(category.HandleMessage(0xFF, data));
        EXPECT_EQ(msg1.handleCallCount, 0);
    }

    TEST(CanCategoryTest, HandleMessageDispatchesCorrectMessageType)
    {
        StubCategory category(0x01);
        StubMessageType msg1(0x01);
        StubMessageType msg2(0x02);
        category.AddMessageType(msg1);
        category.AddMessageType(msg2);

        hal::Can::Message data;
        EXPECT_TRUE(category.HandleMessage(0x02, data));
        EXPECT_EQ(msg1.handleCallCount, 0);
        EXPECT_EQ(msg2.handleCallCount, 1);
    }

    TEST(CanCategoryTest, HandleMessageCanBeCalledMultipleTimes)
    {
        StubCategory category(0x01);
        StubMessageType msg1(0x01);
        category.AddMessageType(msg1);

        hal::Can::Message data;
        category.HandleMessage(0x01, data);
        category.HandleMessage(0x01, data);

        EXPECT_EQ(msg1.handleCallCount, 2);
    }

    // --- CanId construction and extraction ---

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

    // --- CanFrameCodec ---

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

    TEST(CanFrameCodecTest, Fixed32ToFloat_RoundTrip)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(3.14f, 10000);
        float result = CanFrameCodec::Fixed32ToFloat(fixed, 10000);
        EXPECT_NEAR(result, 3.14f, 0.001f);
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
