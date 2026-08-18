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
        MOCK_METHOD(void, SendFrame, (const hal::Can::Message&, const infra::Function<void(bool)>&));
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
            [this](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                mocks.SendFrame(f, d);
            },
            [this](AbortReason r)
            {
                mocks.OnAbort(r);
            });
    }

    void InvokeOnDone(const hal::Can::Message&, const infra::Function<void(bool)>& d)
    {
        d(true);
    }
};

TEST_F(IsoTpSenderTest, Send_SingleFrame_1Byte)
{
    uint8_t pdu[] = { 0xAB };
    bool doneCalled = false;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([&](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0], 0x01u);
                EXPECT_EQ(f[1], 0xABu);
                d(true);
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
        .WillOnce(Invoke([this, &doneCalled](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
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

TEST_F(IsoTpSenderTest, Send_RawSendFails_Aborts)
{
    uint8_t pdu[] = { 0xAB };

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(false);
            }));
    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [] {}));
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_HappyPath_BS0)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    bool doneCalled = false;

    testing::InSequence seq;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([&](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0], 0x10u);
                EXPECT_EQ(f[1], 0x08u);
                d(true);
            }));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [&]
        {
            doneCalled = true;
        }));
    EXPECT_FALSE(sender.IsIdle());

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0u, fc);

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([&](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0], 0x21u);
                EXPECT_EQ(f[1], 7u);
                EXPECT_EQ(f[2], 8u);
                d(true);
            }));

    sender.ProcessFlowControl(fc);

    EXPECT_TRUE(doneCalled);
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, ProcessFlowControl_FCOverflow_Aborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));

    sender.Send(infra::MakeRange(pdu), [] {});

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::overflow, 0u, 0u, fc);

    EXPECT_CALL(mocks, OnAbort(AbortReason::overflow));
    sender.ProcessFlowControl(fc);

    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, ProcessFlowControl_FCWait_RestartsNBsAndKeepsWaiting)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));

    sender.Send(infra::MakeRange(pdu), [] {});

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::wait, 0u, 0u, fc);

    // A single Wait frame must not abort: ISO 15765-2 requires the sender to
    // restart N_Bs and keep waiting, up to N_WFTmax consecutive Wait frames.
    sender.ProcessFlowControl(fc);

    EXPECT_FALSE(sender.IsIdle());

    // Still waiting after almost a full nBs timeout, since the wait frame
    // restarted the timer.
    ForwardTime(std::chrono::milliseconds(999));
    EXPECT_FALSE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, ProcessFlowControl_FCWait_ExceedingLimitAborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));

    sender.Send(infra::MakeRange(pdu), [] {});

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::wait, 0u, 0u, fc);

    for (int i = 0; i != nWftMax; ++i)
        sender.ProcessFlowControl(fc);
    EXPECT_FALSE(sender.IsIdle());

    EXPECT_CALL(mocks, OnAbort(AbortReason::waitLimitExceeded));
    sender.ProcessFlowControl(fc);

    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, ProcessFlowControl_FCWait_ThenContinueToSend_ProceedsNormally)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    bool doneCalled = false;

    testing::InSequence seq;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [&doneCalled]
        {
            doneCalled = true;
        }));

    hal::Can::Message waitFc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::wait, 0u, 0u, waitFc);
    sender.ProcessFlowControl(waitFc);
    EXPECT_FALSE(sender.IsIdle());

    hal::Can::Message ctsFc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0u, ctsFc);

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0], 0x21u);
                d(true);
            }));

    sender.ProcessFlowControl(ctsFc);

    EXPECT_TRUE(doneCalled);
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

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));

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
        .WillOnce(Invoke([](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0], 0x10u);
                d(true);
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
        .WillOnce(Invoke([](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0] & 0xF0u, 0x20u);
                d(true);
            }));

    sender.ProcessFlowControl(fc);

    EXPECT_FALSE(doneCalled);
    EXPECT_FALSE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_EmptyPdu_ReturnsFalse)
{
    EXPECT_FALSE(sender.Send(infra::ConstByteRange{}, [] {}));
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_PduExceeds4095Bytes_ReturnsFalse)
{
    std::array<uint8_t, 0x1000u> pdu;
    pdu.fill(0);
    EXPECT_FALSE(sender.Send(infra::MakeRange(pdu), [] {}));
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, ProcessFlowControl_FCWait_TimeoutAfterRestart_Aborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));

    sender.Send(infra::MakeRange(pdu), [] {});

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::wait, 0u, 0u, fc);
    sender.ProcessFlowControl(fc);

    EXPECT_CALL(mocks, OnAbort(AbortReason::nBsTimeout));
    ForwardTime(std::chrono::milliseconds(1000));
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_RawSendOfFirstFrameFails_Aborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(false);
            }));
    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));

    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [] {}));
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_WithBlockSize_RawSendOfLastCfInBlockFails_Aborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };

    testing::InSequence seq;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));
    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [] {}));

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 1u, 0u, fc);

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(false);
            }));
    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));

    sender.ProcessFlowControl(fc);
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_WithBlockSize_NBsTimeoutAfterLastCfInBlock_Aborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };

    testing::InSequence seq;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));
    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [] {}));

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 1u, 0u, fc);

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));

    sender.ProcessFlowControl(fc);
    EXPECT_FALSE(sender.IsIdle());

    EXPECT_CALL(mocks, OnAbort(AbortReason::nBsTimeout));
    ForwardTime(std::chrono::milliseconds(1000));
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_BS0_RawSendOfCfFails_Aborts)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    testing::InSequence seq;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));
    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [] {}));

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0u, fc);

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(false);
            }));
    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));

    sender.ProcessFlowControl(fc);
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_BS0_MultipleCFs_ContinuesWithoutWaiting)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    bool doneCalled = false;

    testing::InSequence seq;

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message&, const infra::Function<void(bool)>& d)
            {
                d(true);
            }));
    ASSERT_TRUE(sender.Send(infra::MakeRange(pdu), [&doneCalled]
        {
            doneCalled = true;
        }));

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0u, fc);

    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0] & 0x0Fu, 1u);
                d(true);
            }));
    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0] & 0x0Fu, 2u);
                d(true);
            }));

    sender.ProcessFlowControl(fc);

    EXPECT_TRUE(doneCalled);
    EXPECT_TRUE(sender.IsIdle());
}

TEST_F(IsoTpSenderTest, Send_MultiFrame_StMinDelay_DelaysNextCF)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    bool doneCalled = false;

    testing::InSequence seq;

    // FF
    EXPECT_CALL(mocks, SendFrame(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0], 0x10u);
                d(true);
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
        .WillOnce(Invoke([&doneCalled](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                EXPECT_EQ(f[0] & 0xF0u, 0x20u);
                d(true);
                doneCalled = true;
            }));

    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(doneCalled);
    EXPECT_TRUE(sender.IsIdle());
}
