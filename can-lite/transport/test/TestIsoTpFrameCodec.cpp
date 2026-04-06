#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"
#include "can-lite/transport/test/IsoTpTestHelper.hpp"
#include "gtest/gtest.h"

using namespace services::iso_tp;
using namespace services::iso_tp::test;

TEST(IsoTpFrameCodec, DecodeFrameType_SingleFrame)
{
    auto msg = MakeMessage({ 0x01, 0xAA });
    EXPECT_EQ(IsoTpFrameCodec::DecodeFrameType(msg), FrameType::singleFrame);
}

TEST(IsoTpFrameCodec, DecodeFrameType_FirstFrame)
{
    auto msg = MakeMessage({ 0x10, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
    EXPECT_EQ(IsoTpFrameCodec::DecodeFrameType(msg), FrameType::firstFrame);
}

TEST(IsoTpFrameCodec, DecodeFrameType_ConsecutiveFrame)
{
    auto msg = MakeMessage({ 0x21, 0x07 });
    EXPECT_EQ(IsoTpFrameCodec::DecodeFrameType(msg), FrameType::consecutiveFrame);
}

TEST(IsoTpFrameCodec, DecodeFrameType_FlowControl)
{
    auto msg = MakeMessage({ 0x30, 0x00, 0x00 });
    EXPECT_EQ(IsoTpFrameCodec::DecodeFrameType(msg), FrameType::flowControl);
}

TEST(IsoTpFrameCodec, EncodeSingleFrame_1Byte)
{
    uint8_t data[] = { 0xAB };
    hal::Can::Message out;
    ASSERT_TRUE(IsoTpFrameCodec::EncodeSingleFrame(infra::MakeRange(data), out));
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], 0x01);
    EXPECT_EQ(out[1], 0xAB);
}

TEST(IsoTpFrameCodec, EncodeSingleFrame_7Bytes)
{
    uint8_t data[] = { 1, 2, 3, 4, 5, 6, 7 };
    hal::Can::Message out;
    ASSERT_TRUE(IsoTpFrameCodec::EncodeSingleFrame(infra::MakeRange(data), out));
    ASSERT_EQ(out.size(), 8u);
    EXPECT_EQ(out[0], 0x07);
    EXPECT_EQ(out[7], 7u);
}

TEST(IsoTpFrameCodec, EncodeSingleFrame_8Bytes_ReturnsFalse)
{
    uint8_t data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    hal::Can::Message out;
    EXPECT_FALSE(IsoTpFrameCodec::EncodeSingleFrame(infra::MakeRange(data), out));
}

TEST(IsoTpFrameCodec, DecodeSingleFrameLength)
{
    auto msg = MakeMessage({ 0x05, 1, 2, 3, 4, 5 });
    EXPECT_EQ(IsoTpFrameCodec::DecodeSingleFrameLength(msg), 5u);
}

TEST(IsoTpFrameCodec, EncodeFirstFrame_8Bytes)
{
    uint8_t data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    hal::Can::Message out;
    IsoTpFrameCodec::EncodeFirstFrame(infra::MakeRange(data), out);
    ASSERT_EQ(out.size(), 8u);
    EXPECT_EQ(out[0], 0x10);
    EXPECT_EQ(out[1], 0x08);
    EXPECT_EQ(out[2], 1u);
    EXPECT_EQ(out[7], 6u);
}

TEST(IsoTpFrameCodec, DecodeFirstFrameTotalLength)
{
    auto msg = MakeMessage({ 0x10, 0x08, 0, 0, 0, 0, 0, 0 });
    EXPECT_EQ(IsoTpFrameCodec::DecodeFirstFrameTotalLength(msg), 8u);
}

TEST(IsoTpFrameCodec, DecodeFirstFrameTotalLength_MaxValue)
{
    auto msg = MakeMessage({ 0x1F, 0xFF, 0, 0, 0, 0, 0, 0 });
    EXPECT_EQ(IsoTpFrameCodec::DecodeFirstFrameTotalLength(msg), 4095u);
}

TEST(IsoTpFrameCodec, EncodeConsecutiveFrame_SN1)
{
    uint8_t data[] = { 0xAA, 0xBB, 0xCC };
    hal::Can::Message out;
    uint8_t written = IsoTpFrameCodec::EncodeConsecutiveFrame(1u, infra::MakeRange(data), out);
    EXPECT_EQ(written, 3u);
    EXPECT_EQ(out[0], 0x21);
    EXPECT_EQ(out[1], 0xAA);
    EXPECT_EQ(out[3], 0xCC);
}

TEST(IsoTpFrameCodec, EncodeConsecutiveFrame_SN15)
{
    uint8_t data[] = { 0x01 };
    hal::Can::Message out;
    IsoTpFrameCodec::EncodeConsecutiveFrame(15u, infra::MakeRange(data), out);
    EXPECT_EQ(out[0], 0x2F);
}

TEST(IsoTpFrameCodec, DecodeConsecutiveFrameSn)
{
    auto msg = MakeMessage({ 0x23, 0 });
    EXPECT_EQ(IsoTpFrameCodec::DecodeConsecutiveFrameSn(msg), 3u);
}

TEST(IsoTpFrameCodec, EncodeFlowControl_CTS)
{
    hal::Can::Message out;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 10u, out);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0], 0x30);
    EXPECT_EQ(out[1], 0x00);
    EXPECT_EQ(out[2], 10u);
}

TEST(IsoTpFrameCodec, DecodeFlowControl)
{
    auto msg = MakeMessage({ 0x30, 0x05, 0x0A });
    FlowStatus fs;
    uint8_t bs, stMin;
    IsoTpFrameCodec::DecodeFlowControl(msg, fs, bs, stMin);
    EXPECT_EQ(fs, FlowStatus::continueToSend);
    EXPECT_EQ(bs, 5u);
    EXPECT_EQ(stMin, 10u);
}

TEST(IsoTpFrameCodec, StMinToDuration_Zero)
{
    EXPECT_EQ(IsoTpFrameCodec::StMinToDuration(0x00), std::chrono::milliseconds(0));
}

TEST(IsoTpFrameCodec, StMinToDuration_127ms)
{
    EXPECT_EQ(IsoTpFrameCodec::StMinToDuration(0x7F), std::chrono::milliseconds(127));
}

TEST(IsoTpFrameCodec, StMinToDuration_Microseconds)
{
    EXPECT_EQ(IsoTpFrameCodec::StMinToDuration(0xF1), std::chrono::microseconds(100));
    EXPECT_EQ(IsoTpFrameCodec::StMinToDuration(0xF9), std::chrono::microseconds(900));
}

TEST(IsoTpFrameCodec, StMinToDuration_Reserved)
{
    EXPECT_EQ(IsoTpFrameCodec::StMinToDuration(0x80), infra::Duration::zero());
    EXPECT_EQ(IsoTpFrameCodec::StMinToDuration(0xFF), infra::Duration::zero());
}
