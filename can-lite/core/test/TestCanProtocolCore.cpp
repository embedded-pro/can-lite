#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceTable.hpp"
#include "gtest/gtest.h"
#include <limits>

namespace
{
    using namespace services;

    // --- CanCategory ---

    class StubCategoryServer
        : private CanCategoryHandlerStorage<4>
        , public CanCategoryServer
    {
    public:
        explicit StubCategoryServer(uint8_t id)
            : CanCategoryServer(messageTypeStorage)
            , id(id)
        {}

        uint8_t Id() const override
        {
            return id;
        }

        bool RequiresSequenceValidation() const override
        {
            return true;
        }

        void AddCountingHandler(uint8_t messageType, int& count, std::size_t& lastSize)
        {
            AddMessageType(messageType, [&count, &lastSize](infra::ConstByteRange payload)
                {
                    lastSize = payload.size();
                    ++count;
                    return true;
                });
        }

        void AddRejectingHandler(uint8_t messageType)
        {
            AddMessageType(messageType, [](infra::ConstByteRange)
                {
                    return false;
                });
        }

    private:
        uint8_t id;
    };

    class StubCategoryClient
        : private CanCategoryHandlerStorage<2>
        , public CanCategoryClient
    {
    public:
        explicit StubCategoryClient(uint8_t id)
            : CanCategoryClient(messageTypeStorage)
            , id(id)
        {}

        uint8_t Id() const override
        {
            return id;
        }

        bool RequiresSequenceValidation() const override
        {
            return false;
        }

    private:
        uint8_t id;
    };

    TEST(CanCategoryTest, CategoryDeclaresWhetherSequenceValidationIsRequired)
    {
        StubCategoryServer server(0x01);
        StubCategoryClient client(0x01);

        EXPECT_TRUE(server.RequiresSequenceValidation());
        EXPECT_FALSE(client.RequiresSequenceValidation());
    }

    TEST(CanCategoryTest, IdReturnsConfiguredValue)
    {
        StubCategoryServer cat1(0x01);
        StubCategoryServer cat2(0x05);
        StubCategoryServer cat3(0x07);

        EXPECT_EQ(cat1.Id(), 0x01);
        EXPECT_EQ(cat2.Id(), 0x05);
        EXPECT_EQ(cat3.Id(), 0x07);
    }

    TEST(CanCategoryTest, HandleMessageDispatchesToRegisteredMessageType)
    {
        StubCategoryServer category(0x01);
        int count1 = 0;
        int count2 = 0;
        std::size_t size1 = 0;
        std::size_t size2 = 0;
        category.AddCountingHandler(0x01, count1, size1);
        category.AddCountingHandler(0x02, count2, size2);

        uint8_t data[] = { 0xAA, 0xBB };

        EXPECT_EQ(category.HandleMessage(0x01, infra::MakeRange(data)), CanDispatchResult::handled);
        EXPECT_EQ(count1, 1);
        EXPECT_EQ(size1, 2u);
        EXPECT_EQ(count2, 0);
    }

    TEST(CanCategoryTest, HandleMessageReportsUnknownMessageType)
    {
        StubCategoryServer category(0x01);
        int count = 0;
        std::size_t size = 0;
        category.AddCountingHandler(0x01, count, size);

        EXPECT_EQ(category.HandleMessage(0xFF, infra::ConstByteRange()), CanDispatchResult::unknownMessageType);
        EXPECT_EQ(count, 0);
    }

    TEST(CanCategoryTest, HandleMessageReportsRejectionFromHandler)
    {
        StubCategoryServer category(0x01);
        category.AddRejectingHandler(0x03);

        uint8_t data[] = { 0x01 };

        EXPECT_EQ(category.HandleMessage(0x03, infra::MakeRange(data)), CanDispatchResult::rejected);
    }

    TEST(CanCategoryTest, HandleMessageDispatchesCorrectMessageType)
    {
        StubCategoryServer category(0x01);
        int count1 = 0;
        int count2 = 0;
        std::size_t size1 = 0;
        std::size_t size2 = 0;
        category.AddCountingHandler(0x01, count1, size1);
        category.AddCountingHandler(0x02, count2, size2);

        EXPECT_EQ(category.HandleMessage(0x02, infra::ConstByteRange()), CanDispatchResult::handled);
        EXPECT_EQ(count1, 0);
        EXPECT_EQ(count2, 1);
    }

    TEST(CanCategoryTest, HandleMessageCanBeCalledMultipleTimes)
    {
        StubCategoryServer category(0x01);
        int count = 0;
        std::size_t size = 0;
        category.AddCountingHandler(0x01, count, size);

        category.HandleMessage(0x01, infra::ConstByteRange());
        category.HandleMessage(0x01, infra::ConstByteRange());

        EXPECT_EQ(count, 2);
    }

    TEST(CanCategoryTest, HandleMessageAcceptsReassembledPduLargerThanAFrame)
    {
        StubCategoryServer category(0x01);
        int count = 0;
        std::size_t size = 0;
        category.AddCountingHandler(0x10, count, size);

        uint8_t pdu[32] = {};

        EXPECT_EQ(category.HandleMessage(0x10, infra::MakeRange(pdu)), CanDispatchResult::handled);
        EXPECT_EQ(count, 1);
        EXPECT_EQ(size, 32u);
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

    // --- CanSequenceTable ---

    TEST(CanSequenceTableTest, Allocate_StartsAtZeroAndAdvances)
    {
        CanSequenceTable table;

        EXPECT_EQ(table.Allocate(1), 0);
        EXPECT_EQ(table.Allocate(1), 1);
        EXPECT_EQ(table.Allocate(1), 2);
    }

    TEST(CanSequenceTableTest, Allocate_IsIndependentPerPeer)
    {
        CanSequenceTable table;

        EXPECT_EQ(table.Allocate(1), 0);
        EXPECT_EQ(table.Allocate(2), 0);
        EXPECT_EQ(table.Allocate(1), 1);
        EXPECT_EQ(table.Allocate(2), 1);
    }

    TEST(CanSequenceTableTest, Allocate_WrapsAtByteBoundary)
    {
        CanSequenceTable table;

        for (uint16_t i = 0; i != 256u; ++i)
            table.Allocate(1);

        EXPECT_EQ(table.Allocate(1), 0);
    }

    TEST(CanSequenceTableTest, Validate_AdoptsFirstSequenceFromAPeer)
    {
        CanSequenceTable table;

        auto result = table.Validate(1, 200);
        EXPECT_TRUE(result.accepted);
        EXPECT_EQ(result.expected, 200);

        EXPECT_TRUE(table.Validate(1, 201).accepted);
    }

    TEST(CanSequenceTableTest, Validate_RejectsOutOfOrderAndReportsExpectation)
    {
        CanSequenceTable table;

        table.Validate(1, 0);

        auto result = table.Validate(1, 5);
        EXPECT_FALSE(result.accepted);
        EXPECT_EQ(result.expected, 1);
    }

    TEST(CanSequenceTableTest, Validate_DoesNotAdvanceOnRejection)
    {
        CanSequenceTable table;

        table.Validate(1, 0);
        table.Validate(1, 5);

        EXPECT_TRUE(table.Validate(1, 1).accepted);
    }

    TEST(CanSequenceTableTest, Validate_WrapsAtByteBoundary)
    {
        CanSequenceTable table;

        EXPECT_TRUE(table.Validate(1, 255).accepted);
        EXPECT_TRUE(table.Validate(1, 0).accepted);
    }

    TEST(CanSequenceTableTest, Resync_MovesExpectationForOnePeerOnly)
    {
        CanSequenceTable table;

        table.Allocate(1);
        table.Allocate(2);

        table.Resync(1, 42);

        EXPECT_EQ(table.Allocate(1), 42);
        EXPECT_EQ(table.Allocate(2), 1);
    }

    TEST(CanSequenceTableTest, Forget_ClearsAllPeers)
    {
        CanSequenceTable table;

        table.Allocate(1);
        table.Allocate(1);
        table.Forget();

        EXPECT_EQ(table.Allocate(1), 0);
    }

    TEST(CanSequenceTableTest, FullTable_EvictsInsteadOfAborting)
    {
        CanSequenceTable table;

        for (uint16_t peer = 0; peer != CanSequenceTable::maxPeers; ++peer)
        {
            table.Allocate(peer);
            table.Allocate(peer);
        }

        // A ninth peer must be served rather than bringing the node down.
        EXPECT_EQ(table.Allocate(CanSequenceTable::maxPeers), 0);
        EXPECT_EQ(table.Allocate(CanSequenceTable::maxPeers), 1);

        // The peer that lost its slot simply starts over.
        EXPECT_EQ(table.Allocate(0), 0);
    }
}
