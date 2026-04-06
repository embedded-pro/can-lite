#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"
#include "can-lite/transport/iso-tp/IsoTpSender.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace services::iso_tp;
using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace
{
    constexpr uint16_t TestPduSize = 64u;
    using TestSender = IsoTpSender::WithStorage<TestPduSize>;

    struct MockCallbacks
    {
        MOCK_METHOD(void, SendFrame, (const hal::Can::Message&, const infra::Function<void()>&));
        MOCK_METHOD(void, OnAbort, (AbortReason));
    };
}

class IsoTpSenderTest
    : public testing::Test
    , public infra::ClockFixture
{
protected:
    StrictMock<MockCallbacks> mocks;
    TestSender sender;

    void SetUp() override
    {
        sender.Configure(
            [this](const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                mocks.SendFrame(f, d);
            },
            [this](AbortReason r)
            {
                mocks.OnAbort(r);
            });
    }

    void InvokeOnDone(const hal::Can::Message&, const infra::Function<void()>& d)
    {
        d();
    }
};

TEST_F(IsoTpSenderTest, Send_SingleFrame_1Byte)
{
    uint8_t pdu[] = { 0xAB };
    bool doneCalled = false;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([&](const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                EXPECT_EQ(f[0], 0x01u);
                EXPECT_EQ(f[1], 0xABu);
                d();
            }));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [&]
        {
            doneCalled = true;
        }));
    EXPECT_TRUE(doneCalled);
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_SingleFrame_7Bytes)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7 };
    bool doneCalled = false;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([this, &doneCalled](const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                InvokeOnDone(f, d);
                doneCalled = true;
            }));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [&]
        {
            doneCalled = true;
        }));
    EXPECT_TRUE(doneCalled);
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_WhenBusy_ReturnsFalse)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_CALL(mocks, SendFrame(_, _));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [] {}));

    uint8_t pdu2[] = { 9 };
    EXPECT_FALSE(sender.Send(infra::MakeRange(pdu2), [] {}));
}

TEST_F(IsoTpSenderTest, Send_ExceedsMaxPduSize_ReturnsFalse)
{
    std::array<uint8_t, TestPduSize + 1> pdu;
    pdu.fill(0);
    EXPECT_FALSE(sender.Send(infra::MakeRange(pdu), [] {}));
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_HappyPath_BS0)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    bool doneCalled = false;

    testing::InSequence seq;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([&](const hal::Can::Message& f, const infra::Function<void()>&)
            {
                EXPECT_EQ(f[0], 0x10u);
                EXPECT_EQ(f[1], 0x08u);
            }));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [&]
        {
            doneCalled = true;
        }));
    EXPECT_FALSE(sender.IsIdle());

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0u, fc);

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([&](const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                EXPECT_EQ(f[0], 0x21u);
                EXPECT_EQ(f[1], 7u);
                EXPECT_EQ(f[2], 8u);
                d();
            }));

    sender.ProcessFlowControl(fc);

    EXPECT_TRUE(doneCalled);
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, ProcessFlowControl_FCOverflow_Aborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_CALL(mocks, SendFrame(_, _));

    sender.Send(infra::MakeRange(pdu), [] {});

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::overflow, 0u, 0u, fc);

    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));
    sender.ProcessFlowControl(fc);

    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, ProcessFlowControl_FCWait_Aborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_CALL(mocks, SendFrame(_, _));

    sender.Send(infra::MakeRange(pdu), [] {});

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::wait, 0u, 0u, fc);

    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));
    sender.ProcessFlowControl(fc);

    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, ProcessFlowControl_WhenIdle_Ignored)
{
    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0u, fc);
    sender.ProcessFlowControl(fc);
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_nBsTimeout_Aborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_CALL(mocks, SendFrame(_, _));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [] {}));
    EXPECT_FALSE(sender.IsIdle());

    EXPECT_CALL(mocks, OnAbort(AbortReason::nBsTimeout));
    ForwardTime(std::chrono::milliseconds(1000));

    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_WithBlockSize_WaitsForFC)
{
    // 14-byte PDU requires 2 CFs: with BS=1 sender goes back to waitingForFc after CF1
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
    bool doneCalled = false;

    testing::InSequence seq;

    // FF
    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& f, const infra::Function<void()>&)
            {
                EXPECT_EQ(f[0], 0x10u);
            }));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [&doneCalled]
        {
            doneCalled = true;
        }));
    EXPECT_FALSE(sender.IsIdle());

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 1u, 0u, fc);

    // CF1 — invoke callback to trigger nBsTimer start (then sender waits for another FC)
    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                EXPECT_EQ(f[0] & 0xF0u, 0x20u);
                d();
            }));

    sender.ProcessFlowControl(fc);

    EXPECT_FALSE(doneCalled);
    EXPECT_FALSE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_StMinDelay_DelaysNextCF)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    bool doneCalled = false;

    testing::InSequence seq;

    // FF
    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& f, const infra::Function<void()>&)
            {
                EXPECT_EQ(f[0], 0x10u);
            }));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [&doneCalled]
        {
            doneCalled = true;
        }));

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0x01u, fc);

    // CF should NOT be sent before timer fires
    sender.ProcessFlowControl(fc);
    EXPECT_FALSE(doneCalled);

    // Advance 1ms — stMinTimer fires, CF sent
    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([&doneCalled](const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                EXPECT_EQ(f[0] & 0xF0u, 0x20u);
                d();
                doneCalled = true;
            }));

    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(doneCalled);
    EXPECT_TRUE(sender.IsIdle());
}
