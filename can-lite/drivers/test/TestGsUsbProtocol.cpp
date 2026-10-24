#include "can-lite/drivers/codec/GsUsbProtocol.hpp"
#include "gtest/gtest.h"
#include <array>

namespace
{
    using namespace services;

    class TestGsUsbProtocol
        : public testing::Test
    {
    public:
        static gs_usb::BitTimingConstants CandleLight()
        {
            return gs_usb::BitTimingConstants{ 0, 48000000, 1, 16, 1, 8, 4, 1, 1024, 1 };
        }

        static uint32_t Quanta(const gs_usb::BitTiming& timing)
        {
            return 1 + timing.propSeg + timing.phaseSeg1 + timing.phaseSeg2;
        }

        static double SamplePoint(const gs_usb::BitTiming& timing)
        {
            return static_cast<double>(1 + timing.propSeg + timing.phaseSeg1) / Quanta(timing);
        }
    };
}

TEST_F(TestGsUsbProtocol, host_format_is_the_little_endian_marker)
{
    EXPECT_EQ((std::array<uint8_t, 4>{ 0xef, 0xbe, 0x00, 0x00 }), gs_usb::EncodeHostFormat());
}

TEST_F(TestGsUsbProtocol, encodes_mode_and_bit_timing_as_little_endian_words)
{
    EXPECT_EQ((std::array<uint8_t, 8>{ 1, 0, 0, 0, 0, 0, 0, 0 }), gs_usb::EncodeMode(gs_usb::modeStart));
    EXPECT_EQ((std::array<uint8_t, 20>{ 6, 0, 0, 0, 7, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 3, 1, 0, 0 }), gs_usb::EncodeBitTiming(gs_usb::BitTiming{ 6, 7, 2, 1, 259 }));
}

TEST_F(TestGsUsbProtocol, encodes_an_extended_frame_in_the_host_frame_layout)
{
    CanFrame data;
    data.push_back(0xaa);
    data.push_back(0xbb);

    const auto bytes = gs_usb::EncodeFrame(5, 0x12345678, true, data);

    EXPECT_EQ((std::array<uint8_t, 20>{ 5, 0, 0, 0, 0x78, 0x56, 0x34, 0x92, 2, 0, 0, 0, 0xaa, 0xbb, 0, 0, 0, 0, 0, 0 }), bytes);
}

TEST_F(TestGsUsbProtocol, encodes_a_standard_frame_without_the_extended_flag)
{
    const auto bytes = gs_usb::EncodeFrame(0, 0x7ff, false, {});

    EXPECT_EQ(0xff, bytes[4]);
    EXPECT_EQ(0x07, bytes[5]);
    EXPECT_EQ(0x00, bytes[7]);
    EXPECT_EQ(0, bytes[8]);
}

TEST_F(TestGsUsbProtocol, decodes_a_received_frame)
{
    const std::array<uint8_t, 20> bytes{ 0xff, 0xff, 0xff, 0xff, 0x23, 0x01, 0x00, 0x80, 3, 0, 0, 0, 1, 2, 3, 0, 0, 0, 0, 0 };

    const auto frame = gs_usb::DecodeFrame(bytes.data(), bytes.size());

    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(gs_usb::receivedEchoId, frame->echoId);
    EXPECT_EQ(0x123u, frame->id);
    EXPECT_TRUE(frame->extended);
    EXPECT_FALSE(frame->remote);
    EXPECT_FALSE(frame->error);
    ASSERT_EQ(3u, frame->data.size());
    EXPECT_EQ(3, frame->data[2]);
}

TEST_F(TestGsUsbProtocol, decodes_error_and_remote_flags)
{
    const std::array<uint8_t, 20> error{ 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0x20, 8, 0, 0, 0 };
    const std::array<uint8_t, 20> remote{ 0xff, 0xff, 0xff, 0xff, 0x23, 0x01, 0, 0x40, 2, 0, 0, 0 };

    EXPECT_TRUE(gs_usb::DecodeFrame(error.data(), error.size())->error);
    const auto frame = gs_usb::DecodeFrame(remote.data(), remote.size());
    ASSERT_TRUE(frame.has_value());
    EXPECT_TRUE(frame->remote);
    EXPECT_TRUE(frame->data.empty());
}

TEST_F(TestGsUsbProtocol, rejects_short_frames_and_oversized_lengths)
{
    const std::array<uint8_t, 20> oversized{ 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 9 };

    EXPECT_FALSE(gs_usb::DecodeFrame(oversized.data(), 19).has_value());
    EXPECT_FALSE(gs_usb::DecodeFrame(oversized.data(), oversized.size()).has_value());
}

TEST_F(TestGsUsbProtocol, decodes_bit_timing_constants)
{
    const std::array<uint8_t, 40> bytes{ 0, 0, 0, 0, 0x00, 0x6c, 0xdc, 0x02, 1, 0, 0, 0, 16, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 0x00, 0x04, 0, 0, 1, 0, 0, 0 };

    const auto constants = gs_usb::DecodeBitTimingConstants(bytes.data(), bytes.size());

    ASSERT_TRUE(constants.has_value());
    EXPECT_EQ(48000000u, constants->clock);
    EXPECT_EQ(16u, constants->tseg1Max);
    EXPECT_EQ(1024u, constants->brpMax);
    EXPECT_FALSE(gs_usb::DecodeBitTimingConstants(bytes.data(), 39).has_value());
}

TEST_F(TestGsUsbProtocol, rejects_constants_without_a_clock)
{
    const std::array<uint8_t, 40> bytes{};

    EXPECT_FALSE(gs_usb::DecodeBitTimingConstants(bytes.data(), bytes.size()).has_value());
}

TEST_F(TestGsUsbProtocol, computes_exact_bit_timing_near_87_5_percent_for_standard_bitrates)
{
    for (const auto bitrate : { 1000000u, 500000u, 250000u, 125000u })
    {
        const auto timing = gs_usb::ComputeBitTiming(CandleLight(), bitrate);

        ASSERT_TRUE(timing.has_value()) << bitrate;
        EXPECT_EQ(48000000u, Quanta(*timing) * timing->brp * bitrate) << bitrate;
        EXPECT_NEAR(0.875, SamplePoint(*timing), 0.02) << bitrate;
        EXPECT_LE(timing->propSeg + timing->phaseSeg1, 16u) << bitrate;
        EXPECT_LE(timing->phaseSeg2, 8u) << bitrate;
        EXPECT_EQ(1u, timing->sjw) << bitrate;
    }
}

TEST_F(TestGsUsbProtocol, reports_bitrates_the_clock_cannot_divide_exactly)
{
    EXPECT_FALSE(gs_usb::ComputeBitTiming(CandleLight(), 7000).has_value());
}
