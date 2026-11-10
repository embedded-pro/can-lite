#include "can-lite/drivers/codec/SlcanCommandDecoder.hpp"
#include "infra/util/BoundedVector.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::ElementsAre;
    using Command = SlcanCommandDecoder::Command;

    class TestSlcanCommandDecoder
        : public testing::Test
    {
    public:
        using Items = infra::BoundedVector<SlcanCommandDecoder::Item>::WithMaxSize<8>;

        Items Decode(infra::BoundedConstString bytes)
        {
            Items items;
            for (const auto byte : bytes)
                if (const auto item = decoder.Feed(byte))
                    items.push_back(*item);
            return items;
        }

        SlcanCommandDecoder decoder;
    };
}

TEST_F(TestSlcanCommandDecoder, decodes_the_commands_an_adapter_opens_a_channel_with)
{
    const auto items = Decode(SlcanOpenCommands('6'));

    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(items[0].command, Command::close);
    EXPECT_EQ(items[1].command, Command::bitrate);
    EXPECT_EQ(items[1].bitrate, 500000u);
    EXPECT_EQ(items[2].command, Command::open);
}

TEST_F(TestSlcanCommandDecoder, decodes_a_virtual_bus_join_with_its_name)
{
    const auto items = Decode(VirtualBusJoin("bench"));

    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].command, Command::join);
    EXPECT_EQ(items[0].name, infra::BoundedConstString("bench"));
}

TEST_F(TestSlcanCommandDecoder, decodes_frames_as_an_adapter_encodes_them)
{
    CanFrame data;
    data.push_back(0x12);
    data.push_back(0xab);

    const auto items = Decode(EncodeSlcanFrame(0x10a0123, true, data));

    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].command, Command::frame);
    EXPECT_EQ(items[0].frame.id, 0x10a0123u);
    EXPECT_TRUE(items[0].frame.extended);
    EXPECT_THAT(items[0].frame.data, ElementsAre(0x12, 0xab));
}

TEST_F(TestSlcanCommandDecoder, reports_what_it_cannot_decode_and_skips_empty_lines)
{
    const auto items = Decode("S9\rX\rT12\r\r");

    ASSERT_EQ(items.size(), 3u);
    for (const auto& item : items)
        EXPECT_EQ(item.command, Command::unknown);
}

TEST_F(TestSlcanCommandDecoder, refuses_an_empty_or_overlong_bus_name)
{
    EXPECT_TRUE(VirtualBusJoin("").empty());
    EXPECT_TRUE(VirtualBusJoin("a-bus-name-far-longer-than-a-join-line-can-carry").empty());

    const auto items = Decode("@\r");
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].command, Command::unknown);
}

TEST_F(TestSlcanCommandDecoder, keeps_a_partial_command_between_feeds)
{
    EXPECT_TRUE(Decode("S").empty());
    const auto items = Decode("8\r");

    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].bitrate, 1000000u);
}
