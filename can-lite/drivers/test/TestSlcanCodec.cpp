#include "can-lite/drivers/codec/SlcanCodec.hpp"
#include "infra/util/BoundedVector.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <initializer_list>

namespace
{
    using namespace services;
    using testing::ElementsAre;
    using testing::IsEmpty;

    CanFrame Bytes(std::initializer_list<uint8_t> bytes)
    {
        CanFrame frame;
        for (const auto byte : bytes)
            frame.push_back(byte);
        return frame;
    }

    class TestSlcanCodec
        : public testing::Test
    {
    public:
        using Items = infra::BoundedVector<SlcanDecoder::Item>::WithMaxSize<8>;

        Items Decode(infra::BoundedConstString bytes)
        {
            Items items;
            for (const auto byte : bytes)
                if (const auto item = decoder.Feed(byte))
                    items.push_back(*item);
            return items;
        }

        SlcanDecoder decoder;
    };
}

TEST_F(TestSlcanCodec, maps_standard_bitrates_to_their_codes)
{
    EXPECT_EQ('8', SlcanBitrateCode(1000000));
    EXPECT_EQ('7', SlcanBitrateCode(800000));
    EXPECT_EQ('6', SlcanBitrateCode(500000));
    EXPECT_EQ('5', SlcanBitrateCode(250000));
    EXPECT_EQ('4', SlcanBitrateCode(125000));
    EXPECT_EQ('0', SlcanBitrateCode(10000));
}

TEST_F(TestSlcanCodec, rejects_bitrates_slcan_cannot_select)
{
    EXPECT_FALSE(SlcanBitrateCode(83333).has_value());
    EXPECT_FALSE(SlcanBitrateCode(0).has_value());
}

TEST_F(TestSlcanCodec, opens_by_closing_selecting_the_bitrate_and_opening)
{
    EXPECT_EQ("C\rS8\rO\r", SlcanOpenCommands('8'));
    EXPECT_EQ("C\r", SlcanCloseCommand());
}

TEST_F(TestSlcanCodec, encodes_extended_frames_with_eight_identifier_digits)
{
    const auto data = Bytes({ 0x01, 0x02 });

    EXPECT_EQ("T1234567820102\r", EncodeSlcanFrame(0x12345678, true, data));
}

TEST_F(TestSlcanCodec, encodes_standard_frames_with_three_identifier_digits)
{
    const auto data = Bytes({ 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 });

    EXPECT_EQ("t7FF0\r", EncodeSlcanFrame(0x7ff, false, CanFrame{}));
    EXPECT_EQ("t12380011223344556677\r", EncodeSlcanFrame(0x123, false, data));
}

TEST_F(TestSlcanCodec, decodes_standard_and_extended_frames)
{
    const auto items = Decode("t1232ABCD\rT1FFFFFFF0\r");

    ASSERT_EQ(2u, items.size());
    EXPECT_EQ(SlcanDecoder::Event::frame, items[0].event);
    EXPECT_EQ(0x123u, items[0].frame.id);
    EXPECT_FALSE(items[0].frame.extended);
    EXPECT_THAT(items[0].frame.data, ElementsAre(0xab, 0xcd));
    EXPECT_EQ(0x1fffffffu, items[1].frame.id);
    EXPECT_TRUE(items[1].frame.extended);
    EXPECT_THAT(items[1].frame.data, IsEmpty());
}

TEST_F(TestSlcanCodec, keeps_a_partial_line_between_feeds)
{
    EXPECT_THAT(Decode("T0000012"), IsEmpty());
    EXPECT_THAT(Decode("33aa"), IsEmpty());

    const auto items = Decode("bbcc\r");

    ASSERT_EQ(1u, items.size());
    EXPECT_EQ(0x123u, items[0].frame.id);
    EXPECT_THAT(items[0].frame.data, ElementsAre(0xaa, 0xbb, 0xcc));
}

TEST_F(TestSlcanCodec, reports_acknowledgements_and_errors)
{
    const auto items = Decode("\rz\rZ\r\a");

    ASSERT_EQ(4u, items.size());
    EXPECT_EQ(SlcanDecoder::Event::acknowledged, items[0].event);
    EXPECT_EQ(SlcanDecoder::Event::acknowledged, items[1].event);
    EXPECT_EQ(SlcanDecoder::Event::acknowledged, items[2].event);
    EXPECT_EQ(SlcanDecoder::Event::error, items[3].event);
}

TEST_F(TestSlcanCodec, an_error_discards_the_partial_line)
{
    const auto items = Decode("t12\a3\rt1230\r");

    ASSERT_EQ(2u, items.size());
    EXPECT_EQ(SlcanDecoder::Event::error, items[0].event);
    EXPECT_EQ(SlcanDecoder::Event::frame, items[1].event);
    EXPECT_EQ(0x123u, items[1].frame.id);
}

TEST_F(TestSlcanCodec, flags_remote_frames_without_payload)
{
    const auto items = Decode("r1232\rR123456780\r");

    ASSERT_EQ(2u, items.size());
    EXPECT_TRUE(items[0].frame.remote);
    EXPECT_FALSE(items[0].frame.extended);
    EXPECT_THAT(items[0].frame.data, IsEmpty());
    EXPECT_TRUE(items[1].frame.remote);
    EXPECT_TRUE(items[1].frame.extended);
}

TEST_F(TestSlcanCodec, accepts_a_trailing_timestamp)
{
    const auto items = Decode("t1231FF1A2B\r");

    ASSERT_EQ(1u, items.size());
    EXPECT_THAT(items[0].frame.data, ElementsAre(0xff));
}

TEST_F(TestSlcanCodec, skips_malformed_lines_and_keeps_decoding)
{
    const auto items = Decode("V1013\rt8001\rt1239\rt12X0\rT123\rt12320\rT200000000\rt1230\r");

    ASSERT_EQ(1u, items.size());
    EXPECT_EQ(0x123u, items[0].frame.id);
    EXPECT_THAT(items[0].frame.data, IsEmpty());
}

TEST_F(TestSlcanCodec, discards_an_overlong_line_up_to_its_terminator)
{
    for (std::size_t index = 0; index <= SlcanDecoder::maxLineLength; ++index)
        EXPECT_FALSE(decoder.Feed('1').has_value());

    EXPECT_THAT(Decode("\rt1230\r"), testing::SizeIs(1));
}
