#include "can-lite/drivers/codec/SlcanCodec.hpp"
#include "can-lite/drivers/codec/SlcanCommandDecoder.hpp"
#include "can-lite/drivers/hub/VirtualCanBus.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <initializer_list>

namespace
{
    using namespace services;
    using testing::InSequence;
    using testing::StrictMock;

    MATCHER_P(IsText, expected, "")
    {
        return arg == infra::BoundedConstString(expected);
    }

    class VirtualCanParticipantMock
        : public VirtualCanParticipant
    {
    public:
        MOCK_METHOD(void, Write, (infra::BoundedConstString bytes), (override));
    };

    CanFrame Bytes(std::initializer_list<uint8_t> bytes)
    {
        CanFrame frame;
        for (const auto byte : bytes)
            frame.push_back(byte);
        return frame;
    }

    class TestVirtualCanBus
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Send(VirtualCanParticipant& participant, infra::BoundedConstString text)
        {
            bus.Receive(participant, infra::ConstByteRange(reinterpret_cast<const uint8_t*>(text.data()), reinterpret_cast<const uint8_t*>(text.data() + text.size())));
        }

        void Open(StrictMock<VirtualCanParticipantMock>& participant, infra::BoundedConstString name, char bitrateCode = '8')
        {
            ASSERT_TRUE(bus.Attach(participant));
            EXPECT_CALL(participant, Write(IsText("\r"))).Times(3);
            Send(participant, VirtualBusJoin(name));
            Send(participant, SlcanOpenCommands(bitrateCode));
            testing::Mock::VerifyAndClearExpectations(&participant);
        }

        VirtualCanBus bus;
        StrictMock<VirtualCanParticipantMock> a;
        StrictMock<VirtualCanParticipantMock> b;
        StrictMock<VirtualCanParticipantMock> c;
    };
}

TEST_F(TestVirtualCanBus, a_frame_reaches_the_other_open_participants_after_its_transmission_time)
{
    Open(a, "bench");
    Open(b, "bench");

    Send(a, EncodeSlcanFrame(0x1abcdef, true, Bytes({ 1, 2, 3, 4, 5, 6, 7, 8 })));
    ForwardTime(std::chrono::milliseconds(0));

    EXPECT_CALL(b, Write(IsText("T01ABCDEF80102030405060708\r")));
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(TestVirtualCanBus, participants_on_another_bus_receive_nothing)
{
    Open(a, "bench");
    Open(b, "bench");
    Open(c, "other");

    EXPECT_CALL(b, Write(IsText("T000001231AA\r")));
    Send(a, EncodeSlcanFrame(0x123, true, Bytes({ 0xaa })));
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(TestVirtualCanBus, a_participant_that_closed_its_channel_receives_nothing)
{
    Open(a, "bench");
    Open(b, "bench");
    EXPECT_CALL(b, Write(IsText("\r")));
    Send(b, SlcanCloseCommand());

    Send(a, EncodeSlcanFrame(0x123, true, Bytes({ 0xaa })));
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(TestVirtualCanBus, a_bitrate_other_than_the_open_participants_is_refused)
{
    Open(a, "bench", '8');
    ASSERT_TRUE(bus.Attach(b));

    {
        InSequence sequence;
        EXPECT_CALL(b, Write(IsText("\r")));
        EXPECT_CALL(b, Write(IsText("\a"))).Times(2);
    }
    Send(b, VirtualBusJoin("bench"));
    Send(b, SlcanOpenCommands('6'));
}

TEST_F(TestVirtualCanBus, frames_waiting_for_the_bus_go_lowest_identifier_first)
{
    Open(a, "bench");
    Open(b, "bench");
    Open(c, "bench");

    Send(a, EncodeSlcanFrame(0x300, true, Bytes({ 1 })));
    Send(a, EncodeSlcanFrame(0x200, true, Bytes({ 2 })));
    Send(c, EncodeSlcanFrame(0x100, true, Bytes({ 3 })));

    {
        InSequence sequence;
        EXPECT_CALL(b, Write(IsText("T00000300101\r")));
        EXPECT_CALL(b, Write(IsText("T00000100103\r")));
        EXPECT_CALL(b, Write(IsText("T00000200102\r")));
    }
    EXPECT_CALL(c, Write(IsText("T00000300101\r")));
    EXPECT_CALL(c, Write(IsText("T00000200102\r")));
    EXPECT_CALL(a, Write(IsText("T00000100103\r")));
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(TestVirtualCanBus, the_bus_paces_frames_at_its_bitrate)
{
    Open(a, "slow", '0');
    Open(b, "slow", '0');

    Send(a, EncodeSlcanFrame(0x123, true, Bytes({ 1, 2, 3, 4, 5, 6, 7, 8 })));
    Send(a, EncodeSlcanFrame(0x124, true, Bytes({ 1, 2, 3, 4, 5, 6, 7, 8 })));

    ForwardTime(std::chrono::microseconds(15900));
    EXPECT_CALL(b, Write(IsText("T0000012380102030405060708\r")));
    ForwardTime(std::chrono::microseconds(200));

    ForwardTime(std::chrono::microseconds(15800));
    EXPECT_CALL(b, Write(IsText("T0000012480102030405060708\r")));
    ForwardTime(std::chrono::microseconds(300));
}

TEST_F(TestVirtualCanBus, a_frame_before_the_channel_is_open_is_refused)
{
    ASSERT_TRUE(bus.Attach(a));
    EXPECT_CALL(a, Write(IsText("\a")));
    Send(a, EncodeSlcanFrame(0x123, true, Bytes({ 1 })));
}

TEST_F(TestVirtualCanBus, a_bitrate_or_open_before_joining_a_bus_is_refused)
{
    ASSERT_TRUE(bus.Attach(a));
    {
        InSequence sequence;
        EXPECT_CALL(a, Write(IsText("\r")));
        EXPECT_CALL(a, Write(IsText("\a"))).Times(2);
    }
    Send(a, SlcanOpenCommands('8'));
}

TEST_F(TestVirtualCanBus, a_second_join_is_refused)
{
    Open(a, "bench");
    EXPECT_CALL(a, Write(IsText("\a")));
    Send(a, VirtualBusJoin("other"));
}

TEST_F(TestVirtualCanBus, joining_beyond_the_bus_capacity_is_refused)
{
    std::array<StrictMock<VirtualCanParticipantMock>, VirtualCanBus::maxBuses> others;
    for (std::size_t index = 0; index != others.size(); ++index)
    {
        ASSERT_TRUE(bus.Attach(others[index]));
        Send(others[index], VirtualBusJoin(std::array{ "b0", "b1", "b2", "b3" }[index]));
    }

    ASSERT_TRUE(bus.Attach(a));
    EXPECT_CALL(a, Write(IsText("\a")));
    Send(a, VirtualBusJoin("b4"));
}

TEST_F(TestVirtualCanBus, an_unknown_command_is_refused)
{
    ASSERT_TRUE(bus.Attach(a));
    EXPECT_CALL(a, Write(IsText("\a")));
    Send(a, "V\r");
}

TEST_F(TestVirtualCanBus, a_participant_that_leaves_takes_its_waiting_frames_with_it)
{
    Open(a, "bench");
    Open(b, "bench");
    Open(c, "bench");

    Send(a, EncodeSlcanFrame(0x100, true, Bytes({ 1 })));
    Send(c, EncodeSlcanFrame(0x200, true, Bytes({ 2 })));
    bus.Detach(c);

    EXPECT_CALL(b, Write(IsText("T00000100101\r")));
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(TestVirtualCanBus, a_full_queue_refuses_the_frame)
{
    Open(a, "bench", '0');
    Open(b, "bench", '0');

    for (std::size_t frame = 0; frame != VirtualCanBus::maxQueuedFrames + 1; ++frame)
        Send(a, EncodeSlcanFrame(0x100, true, Bytes({ 1 })));

    EXPECT_CALL(a, Write(IsText("\a")));
    Send(a, EncodeSlcanFrame(0x100, true, Bytes({ 1 })));

    EXPECT_CALL(b, Write(IsText("T00000100101\r"))).Times(VirtualCanBus::maxQueuedFrames + 1);
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(TestVirtualCanBus, refuses_participants_beyond_its_capacity)
{
    std::array<StrictMock<VirtualCanParticipantMock>, VirtualCanBus::maxParticipants> participants;
    for (auto& participant : participants)
        EXPECT_TRUE(bus.Attach(participant));

    EXPECT_FALSE(bus.Attach(a));
}
