#include "can-lite/core/CanPayload.hpp"
#include "gtest/gtest.h"
#include <limits>

namespace
{
    using namespace services;

    hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
    {
        hal::Can::Message message;
        for (auto byte : bytes)
            message.push_back(byte);
        return message;
    }

    TEST(CanPayloadWriterTest, NewWriterIsValidAndEmpty)
    {
        CanPayloadWriter writer;

        EXPECT_TRUE(writer.Valid());
        EXPECT_TRUE(writer.Message().empty());
    }

    TEST(CanPayloadWriterTest, WriteUInt8AppendsSingleByte)
    {
        CanPayloadWriter writer;
        writer.WriteUInt8(0xAB);

        EXPECT_TRUE(writer.Valid());
        EXPECT_EQ(writer.Message(), MakeMessage({ 0xAB }));
    }

    TEST(CanPayloadWriterTest, WriteInt16IsBigEndian)
    {
        CanPayloadWriter writer;
        writer.WriteInt16(-2);

        EXPECT_EQ(writer.Message(), MakeMessage({ 0xFF, 0xFE }));
    }

    TEST(CanPayloadWriterTest, WriteUInt16IsBigEndian)
    {
        CanPayloadWriter writer;
        writer.WriteUInt16(0x1234);

        EXPECT_EQ(writer.Message(), MakeMessage({ 0x12, 0x34 }));
    }

    TEST(CanPayloadWriterTest, WriteInt32IsBigEndian)
    {
        CanPayloadWriter writer;
        writer.WriteInt32(-2);

        EXPECT_EQ(writer.Message(), MakeMessage({ 0xFF, 0xFF, 0xFF, 0xFE }));
    }

    TEST(CanPayloadWriterTest, WriteUInt32IsBigEndian)
    {
        CanPayloadWriter writer;
        writer.WriteUInt32(0x12345678);

        EXPECT_EQ(writer.Message(), MakeMessage({ 0x12, 0x34, 0x56, 0x78 }));
    }

    TEST(CanPayloadWriterTest, WriteFixed16ScalesValue)
    {
        CanPayloadWriter writer;
        writer.WriteFixed16(1.5f, 100);

        EXPECT_EQ(writer.Message(), MakeMessage({ 0x00, 0x96 }));
    }

    TEST(CanPayloadWriterTest, WriteFixed16SaturatesInsteadOfWrapping)
    {
        CanPayloadWriter writer;
        writer.WriteFixed16(1000.0f, 1000);

        EXPECT_EQ(writer.Message(), MakeMessage({ 0x7F, 0xFF }));
    }

    TEST(CanPayloadWriterTest, WriteBytesAppendsWholeRange)
    {
        auto source = MakeMessage({ 0x01, 0x02, 0x03 });

        CanPayloadWriter writer;
        writer.WriteUInt8(0xFF).WriteBytes(infra::MakeRange(source));

        EXPECT_EQ(writer.Message(), MakeMessage({ 0xFF, 0x01, 0x02, 0x03 }));
    }

    TEST(CanPayloadWriterTest, WritesChainInOrder)
    {
        CanPayloadWriter writer;
        writer.WriteUInt8(0x01).WriteUInt16(0x0203).WriteInt16(0x0405);

        EXPECT_TRUE(writer.Valid());
        EXPECT_EQ(writer.Message(), MakeMessage({ 0x01, 0x02, 0x03, 0x04, 0x05 }));
    }

    TEST(CanPayloadWriterTest, FillingTheFrameExactlyStaysValid)
    {
        CanPayloadWriter writer;
        writer.WriteUInt32(0x01020304).WriteUInt32(0x05060708);

        EXPECT_TRUE(writer.Valid());
        EXPECT_EQ(writer.Message().size(), 8u);
    }

    TEST(CanPayloadWriterTest, WriteBeyondFrameInvalidatesAndDiscards)
    {
        CanPayloadWriter writer;
        writer.WriteUInt32(0x01020304).WriteUInt32(0x05060708).WriteUInt8(0x09);

        EXPECT_FALSE(writer.Valid());
        EXPECT_EQ(writer.Message().size(), 8u);
    }

    TEST(CanPayloadWriterTest, PartiallyFittingWriteIsDiscardedWhole)
    {
        CanPayloadWriter writer;
        writer.WriteUInt32(0x01020304).WriteUInt16(0x0506).WriteUInt32(0x0708090A);

        EXPECT_FALSE(writer.Valid());
        EXPECT_EQ(writer.Message(), MakeMessage({ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 }));
    }

    TEST(CanPayloadWriterTest, WriteBytesBeyondFrameIsDiscardedWhole)
    {
        auto source = MakeMessage({ 0x01, 0x02, 0x03 });

        CanPayloadWriter writer;
        writer.WriteUInt32(0x01020304).WriteUInt16(0x0506).WriteBytes(infra::MakeRange(source));

        EXPECT_FALSE(writer.Valid());
        EXPECT_EQ(writer.Message().size(), 6u);
    }

    TEST(CanPayloadWriterTest, InvalidWriterIgnoresFurtherWrites)
    {
        CanPayloadWriter writer;
        writer.WriteUInt32(0x01020304).WriteUInt32(0x05060708).WriteUInt8(0x09);
        ASSERT_FALSE(writer.Valid());

        writer.WriteUInt8(0x0A);

        EXPECT_FALSE(writer.Valid());
        EXPECT_EQ(writer.Message().size(), 8u);
    }

    TEST(CanPayloadReaderTest, ReadUInt8ReturnsBytesInOrder)
    {
        auto message = MakeMessage({ 0x01, 0x02 });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.ReadUInt8(), 0x01);
        EXPECT_EQ(reader.ReadUInt8(), 0x02);
        EXPECT_TRUE(reader.Valid());
    }

    TEST(CanPayloadReaderTest, ReadInt16IsBigEndian)
    {
        auto message = MakeMessage({ 0xFF, 0xFE });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.ReadInt16(), -2);
    }

    TEST(CanPayloadReaderTest, ReadUInt16IsBigEndian)
    {
        auto message = MakeMessage({ 0x12, 0x34 });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.ReadUInt16(), 0x1234);
    }

    TEST(CanPayloadReaderTest, ReadInt32IsBigEndian)
    {
        auto message = MakeMessage({ 0xFF, 0xFF, 0xFF, 0xFE });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.ReadInt32(), -2);
    }

    TEST(CanPayloadReaderTest, ReadUInt32IsBigEndian)
    {
        auto message = MakeMessage({ 0x12, 0x34, 0x56, 0x78 });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.ReadUInt32(), 0x12345678u);
    }

    TEST(CanPayloadReaderTest, ReadFixed16UnscalesValue)
    {
        auto message = MakeMessage({ 0x00, 0x96 });
        CanPayloadReader reader{ message };

        EXPECT_FLOAT_EQ(reader.ReadFixed16(100), 1.5f);
    }

    TEST(CanPayloadReaderTest, SkipAdvancesCursor)
    {
        auto message = MakeMessage({ 0x01, 0x02, 0x03 });
        CanPayloadReader reader{ message };

        reader.Skip(2);

        EXPECT_EQ(reader.ReadUInt8(), 0x03);
        EXPECT_TRUE(reader.Valid());
    }

    TEST(CanPayloadReaderTest, SkipBeyondEndInvalidates)
    {
        auto message = MakeMessage({ 0x01 });
        CanPayloadReader reader{ message };

        reader.Skip(2);

        EXPECT_FALSE(reader.Valid());
    }

    TEST(CanPayloadReaderTest, AvailableTracksRemainingBytes)
    {
        auto message = MakeMessage({ 0x01, 0x02, 0x03 });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.Available(), 3u);
        reader.ReadUInt8();
        EXPECT_EQ(reader.Available(), 2u);
    }

    TEST(CanPayloadReaderTest, AvailableIsZeroOnceInvalid)
    {
        auto message = MakeMessage({ 0x01 });
        CanPayloadReader reader{ message };

        reader.ReadUInt32();

        ASSERT_FALSE(reader.Valid());
        EXPECT_EQ(reader.Available(), 0u);
    }

    TEST(CanPayloadReaderTest, ReadRemainingReturnsRestAndConsumesIt)
    {
        auto message = MakeMessage({ 0x01, 0x02, 0x03 });
        CanPayloadReader reader{ message };
        reader.Skip(1);

        auto remaining = reader.ReadRemaining();

        ASSERT_EQ(remaining.size(), 2u);
        EXPECT_EQ(remaining[0], 0x02);
        EXPECT_EQ(remaining[1], 0x03);
        EXPECT_EQ(reader.Available(), 0u);
    }

    TEST(CanPayloadReaderTest, ReadRemainingOnExhaustedPayloadIsEmpty)
    {
        auto message = MakeMessage({ 0x01 });
        CanPayloadReader reader{ message };
        reader.ReadUInt8();

        EXPECT_TRUE(reader.ReadRemaining().empty());
        EXPECT_TRUE(reader.Valid());
    }

    TEST(CanPayloadReaderTest, ReadRemainingOnInvalidReaderIsEmpty)
    {
        auto message = MakeMessage({ 0x01 });
        CanPayloadReader reader{ message };
        reader.ReadUInt32();

        ASSERT_FALSE(reader.Valid());
        EXPECT_TRUE(reader.ReadRemaining().empty());
    }

    TEST(CanPayloadReaderTest, ReadBeyondEndReturnsZeroAndInvalidates)
    {
        auto message = MakeMessage({ 0x01 });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.ReadUInt32(), 0u);
        EXPECT_FALSE(reader.Valid());
    }

    TEST(CanPayloadReaderTest, ReadInt32BeyondEndReturnsZeroAndInvalidates)
    {
        auto message = MakeMessage({ 0x01, 0x02 });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.ReadInt32(), 0);
        EXPECT_FALSE(reader.Valid());
    }

    TEST(CanPayloadReaderTest, ReadInt16BeyondEndReturnsZeroAndInvalidates)
    {
        auto message = MakeMessage({ 0x01 });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.ReadInt16(), 0);
        EXPECT_FALSE(reader.Valid());
    }

    TEST(CanPayloadReaderTest, ReadUInt16BeyondEndReturnsZeroAndInvalidates)
    {
        auto message = MakeMessage({ 0x01 });
        CanPayloadReader reader{ message };

        EXPECT_EQ(reader.ReadUInt16(), 0u);
        EXPECT_FALSE(reader.Valid());
    }

    TEST(CanPayloadReaderTest, ReadFixed16BeyondEndReturnsZeroAndInvalidates)
    {
        auto message = MakeMessage({ 0x01 });
        CanPayloadReader reader{ message };

        EXPECT_FLOAT_EQ(reader.ReadFixed16(100), 0.0f);
        EXPECT_FALSE(reader.Valid());
    }
}

TEST(CanPayloadReaderTest, InvalidReaderStaysInvalidAndReturnsZero)
{
    auto message = MakeMessage({ 0x01, 0x02, 0x03, 0x04, 0x05 });
    CanPayloadReader reader{ message };

    reader.ReadUInt32();
    ASSERT_TRUE(reader.Valid());

    EXPECT_EQ(reader.ReadUInt32(), 0u);
    ASSERT_FALSE(reader.Valid());

    EXPECT_EQ(reader.ReadUInt8(), 0);
    EXPECT_FALSE(reader.Valid());
}

TEST(CanPayloadReaderTest, PartialReadDoesNotConsumeRemainingBytes)
{
    auto message = MakeMessage({ 0x01, 0x02, 0x03 });
    CanPayloadReader reader{ message };

    reader.ReadUInt32();

    ASSERT_FALSE(reader.Valid());
    EXPECT_EQ(reader.Available(), 0u);
}

TEST(CanPayloadTest, WriterOutputIsReadableByReader)
{
    CanPayloadWriter writer;
    writer.WriteUInt8(0x01).WriteInt16(-300).WriteUInt32(0xDEADBEEF);

    CanPayloadReader reader{ writer.Message() };

    EXPECT_EQ(reader.ReadUInt8(), 0x01);
    EXPECT_EQ(reader.ReadInt16(), -300);
    EXPECT_EQ(reader.ReadUInt32(), 0xDEADBEEFu);
    EXPECT_TRUE(reader.Valid());
    EXPECT_EQ(reader.Available(), 0u);
}
