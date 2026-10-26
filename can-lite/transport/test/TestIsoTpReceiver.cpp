#include "can-lite/transport/iso-tp/IsoTpReceiver.hpp"
#include "can-lite/transport/test/IsoTpTestHelper.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace services::iso_tp;
using namespace services::iso_tp::test;
using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace
{
    constexpr uint16_t TestPduSize = 64u;
    using TestReceiver = IsoTpReceiver::WithStorage<TestPduSize>;

    struct MockCallbacks
    {
        MOCK_METHOD(void, SendFc, (const hal::Can::Message&, const infra::Function<void(bool)>&));
        MOCK_METHOD(void, OnPduReady, (infra::ConstByteRange));
        MOCK_METHOD(void, OnAbort, (AbortReason));
    };
}

class IsoTpReceiverTest
    : public testing::Test
    , public infra::ClockFixture
{
protected:
    StrictMock<MockCallbacks> mocks;
    TestReceiver receiver;

    void SetUp() override
    {
        receiver.Configure(
            [this](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
            {
                mocks.SendFc(f, d);
            },
            [this](infra::ConstByteRange pdu)
            {
                mocks.OnPduReady(pdu);
            },
            [this](AbortReason r)
            {
                mocks.OnAbort(r);
            });
    }
};

TEST_F(IsoTpReceiverTest, Receive_SingleFrame_1Byte)
{
    auto msg = MakeMessage({ 0x01, 0xAB });

    EXPECT_CALL(mocks, OnPduReady(_))
        .WillOnce(Invoke([](infra::ConstByteRange pdu)
            {
                ASSERT_EQ(pdu.size(), 1u);
                EXPECT_EQ(pdu[0], 0xABu);
            }));

    receiver.ProcessFrame(msg);
    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_SingleFrame_7Bytes)
{
    auto msg = MakeMessage({ 0x07, 1, 2, 3, 4, 5, 6, 7 });

    EXPECT_CALL(mocks, OnPduReady(_))
        .WillOnce(Invoke([](infra::ConstByteRange pdu)
            {
                ASSERT_EQ(pdu.size(), 7u);
            }));

    receiver.ProcessFrame(msg);
}

TEST_F(IsoTpReceiverTest, Receive_SingleFrame_ZeroLength_Ignored)
{
    receiver.ProcessFrame(MakeMessage({ 0x00 }));

    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_SingleFrame_LengthBeyondTheFrame_Ignored)
{
    receiver.ProcessFrame(MakeMessage({ 0x05, 1, 2 }));

    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_InvalidSingleFrame_WhileReceivingCF_LeavesTheReceptionRunning)
{
    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 }));

    receiver.ProcessFrame(MakeMessage({ 0x00 }));
    EXPECT_FALSE(receiver.IsIdle());

    EXPECT_CALL(mocks, OnPduReady(_));
    receiver.ProcessFrame(MakeMessage({ 0x21, 7, 8 }));
}

TEST_F(IsoTpReceiverTest, Receive_FirstFrame_SendsFC)
{
    auto msg = MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 });

    EXPECT_CALL(mocks, SendFc(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& fc, const infra::Function<void(bool)>&)
            {
                EXPECT_EQ(fc[0], 0x30u);
                EXPECT_EQ(fc[1], 0x00u);
                EXPECT_EQ(fc[2], 0x00u);
            }));

    receiver.ProcessFrame(msg);
    EXPECT_FALSE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_MultiFrame_TwoFrames)
{
    auto ff = MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 });
    auto cf = MakeMessage({ 0x21, 7, 8 });

    EXPECT_CALL(mocks, SendFc(_, _));

    receiver.ProcessFrame(ff);

    EXPECT_CALL(mocks, OnPduReady(_))
        .WillOnce(Invoke([](infra::ConstByteRange pdu)
            {
                ASSERT_EQ(pdu.size(), 8u);
                EXPECT_EQ(pdu[0], 1u);
                EXPECT_EQ(pdu[7], 8u);
            }));

    receiver.ProcessFrame(cf);
    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_CF_WrongSN_Aborts)
{
    auto ff = MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 });
    auto cfBadSn = MakeMessage({ 0x22, 7, 8 });

    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(ff);

    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));
    receiver.ProcessFrame(cfBadSn);
}

TEST_F(IsoTpReceiverTest, Receive_PDU_ExceedsMaxSize_SendsOverflowFC)
{
    auto ff = MakeMessage({ 0x10, 0x64, 1, 2, 3, 4, 5, 6 });

    EXPECT_CALL(mocks, SendFc(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& fc, const infra::Function<void(bool)>&)
            {
                EXPECT_EQ(fc[0], 0x32u);
            }));

    EXPECT_CALL(mocks, OnAbort(AbortReason::overflow));
    receiver.ProcessFrame(ff);
}

TEST_F(IsoTpReceiverTest, FlowControlFrame_WhileIdle_Ignored)
{
    auto fc = MakeMessage({ 0x30, 0x00, 0x00 });
    receiver.ProcessFrame(fc);
    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_SingleFrame_WhileReceivingCF_ReportsTheInterruptedReceptionThenDeliversIt)
{
    auto ff = MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 });
    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(ff);
    EXPECT_FALSE(receiver.IsIdle());

    testing::InSequence order;
    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));
    EXPECT_CALL(mocks, OnPduReady(_))
        .WillOnce(Invoke([](infra::ConstByteRange pdu)
            {
                ASSERT_EQ(pdu.size(), 1u);
                EXPECT_EQ(pdu[0], 0xABu);
            }));

    receiver.ProcessFrame(MakeMessage({ 0x01, 0xAB }));
    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_FirstFrame_WhileReceivingCF_ReportsTheInterruptedReceptionThenStartsAgain)
{
    EXPECT_CALL(mocks, SendFc(_, _)).Times(2);
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 }));

    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x09, 9, 9, 9, 9, 9, 9 }));
    EXPECT_FALSE(receiver.IsIdle());

    EXPECT_CALL(mocks, OnPduReady(_))
        .WillOnce(Invoke([](infra::ConstByteRange pdu)
            {
                ASSERT_EQ(pdu.size(), 9u);
                EXPECT_EQ(pdu[0], 9u);
                EXPECT_EQ(pdu[8], 0xEEu);
            }));
    receiver.ProcessFrame(MakeMessage({ 0x21, 0xEE, 0xEE, 0xEE }));
}

TEST_F(IsoTpReceiverTest, Receive_SingleFrame_ExceedsMaxSize_Aborts)
{
    // Use a receiver with max size 4 so a 5-byte payload overflows
    using SmallReceiver = IsoTpReceiver::WithStorage<4>;
    StrictMock<MockCallbacks> smallMocks;
    SmallReceiver smallReceiver;
    smallReceiver.Configure(
        [&](const hal::Can::Message& f, const infra::Function<void(bool)>& d)
        {
            smallMocks.SendFc(f, d);
        },
        [&](infra::ConstByteRange pdu)
        {
            smallMocks.OnPduReady(pdu);
        },
        [&](AbortReason r)
        {
            smallMocks.OnAbort(r);
        });

    auto sf = MakeMessage({ 0x05, 1, 2, 3, 4, 5 });

    EXPECT_CALL(smallMocks, OnAbort(AbortReason::overflow));
    smallReceiver.ProcessFrame(sf);
}

TEST_F(IsoTpReceiverTest, Receive_FirstFrame_TooShort_Ignored)
{
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x08, 1, 2, 3 }));

    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_FirstFrame_LengthTooSmall_Ignored)
{
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x05, 1, 2, 3, 4, 5, 0 }));

    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_nCrTimeout_Aborts)
{
    auto ff = MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 });

    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(ff);
    EXPECT_FALSE(receiver.IsIdle());

    EXPECT_CALL(mocks, OnAbort(AbortReason::nCrTimeout));
    ForwardTime(std::chrono::milliseconds(1000));
    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, ProcessFrame_UnknownFrameType_Ignored)
{
    receiver.ProcessFrame(MakeMessage({ 0x40, 0x00 }));

    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, ProcessFrame_UnknownFrameType_WhileReceivingCF_LeavesTheReceptionRunning)
{
    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 }));

    receiver.ProcessFrame(MakeMessage({ 0x40, 0x00 }));

    EXPECT_CALL(mocks, OnPduReady(_));
    receiver.ProcessFrame(MakeMessage({ 0x21, 7, 8 }));
}

TEST_F(IsoTpReceiverTest, Reset_EndsAReceptionInProgressSilently)
{
    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 }));

    receiver.Reset();
    ForwardTime(std::chrono::seconds(2));

    EXPECT_TRUE(receiver.IsIdle());
    receiver.ProcessFrame(MakeMessage({ 0x21, 7, 8 }));
}

TEST_F(IsoTpReceiverTest, LateFlowControlFailure_AfterTheReceptionCompleted_IsIgnored)
{
    infra::Function<void(bool)> flowControlDone;
    EXPECT_CALL(mocks, SendFc(_, _)).WillOnce(testing::SaveArg<1>(&flowControlDone));
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 }));

    EXPECT_CALL(mocks, OnPduReady(_));
    receiver.ProcessFrame(MakeMessage({ 0x21, 7, 8 }));

    flowControlDone(false);
}

TEST_F(IsoTpReceiverTest, Receive_MultiFrame_nCrTimeoutAfterValidCf_Aborts)
{
    auto ff = MakeMessage({ 0x10, 0x0F, 1, 2, 3, 4, 5, 6 });
    auto cf1 = MakeMessage({ 0x21, 7, 8, 9, 10, 11, 12, 13 });

    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(ff);
    EXPECT_FALSE(receiver.IsIdle());

    receiver.ProcessFrame(cf1);
    EXPECT_FALSE(receiver.IsIdle());

    EXPECT_CALL(mocks, OnAbort(AbortReason::nCrTimeout));
    ForwardTime(std::chrono::milliseconds(1000));
    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_MultiFrame_PaddedFinalCf_StopsAtExpectedLength)
{
    auto ff = MakeMessage({ 0x10, 0x09, 1, 2, 3, 4, 5, 6 });
    auto cf1 = MakeMessage({ 0x21, 7, 8, 9, 0xFF, 0xFF, 0xFF, 0xFF });

    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(ff);

    EXPECT_CALL(mocks, OnPduReady(_))
        .WillOnce(Invoke([](infra::ConstByteRange pdu)
            {
                ASSERT_EQ(pdu.size(), 9u);
                EXPECT_EQ(pdu[6], 7u);
                EXPECT_EQ(pdu[7], 8u);
                EXPECT_EQ(pdu[8], 9u);
            }));
    receiver.ProcessFrame(cf1);
    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_ConsecutiveFrame_WhileIdle_Ignored)
{
    auto cf = MakeMessage({ 0x21, 0x01, 0x02 });
    receiver.ProcessFrame(cf);
    EXPECT_TRUE(receiver.IsIdle());
}

TEST_F(IsoTpReceiverTest, Receive_ShortConsecutiveFrameThatIsNotTheLast_Ignored)
{
    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x10, 1, 2, 3, 4, 5, 6 }));

    receiver.ProcessFrame(MakeMessage({ 0x21, 7, 8 }));

    EXPECT_CALL(mocks, OnPduReady(_))
        .WillOnce(Invoke([](infra::ConstByteRange pdu)
            {
                ASSERT_EQ(pdu.size(), 16u);
                EXPECT_EQ(pdu[6], 7u);
                EXPECT_EQ(pdu[15], 16u);
            }));
    receiver.ProcessFrame(MakeMessage({ 0x21, 7, 8, 9, 10, 11, 12, 13 }));
    receiver.ProcessFrame(MakeMessage({ 0x22, 14, 15, 16 }));
}

TEST_F(IsoTpReceiverTest, Receive_LastConsecutiveFrameShorterThanTheRemainder_Ignored)
{
    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x09, 1, 2, 3, 4, 5, 6 }));

    receiver.ProcessFrame(MakeMessage({ 0x21, 7, 8 }));
    EXPECT_FALSE(receiver.IsIdle());

    EXPECT_CALL(mocks, OnPduReady(_));
    receiver.ProcessFrame(MakeMessage({ 0x21, 7, 8, 9 }));
}

TEST_F(IsoTpReceiverTest, Receive_ConsecutiveFramesWithoutData_DoNotKeepTheReceptionAlive)
{
    EXPECT_CALL(mocks, SendFc(_, _));
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 }));

    for (uint8_t sn = 1; sn != 10; ++sn)
    {
        ForwardTime(std::chrono::milliseconds(100));
        receiver.ProcessFrame(MakeMessage({ static_cast<uint8_t>(0x20 | sn) }));
    }

    EXPECT_CALL(mocks, OnAbort(AbortReason::nCrTimeout));
    ForwardTime(std::chrono::milliseconds(100));
}

TEST_F(IsoTpReceiverTest, FlowControl_AdvertisesTheConfiguredBlockSizeAndSeparationTime)
{
    receiver.SetFlowControlParameters(2, 5);

    EXPECT_CALL(mocks, SendFc(testing::ElementsAre(0x30, 2, 5), _));
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x20, 1, 2, 3, 4, 5, 6 }));
}

TEST_F(IsoTpReceiverTest, FlowControl_IsSentAgainAfterEachBlock)
{
    receiver.SetFlowControlParameters(2, 0);

    EXPECT_CALL(mocks, SendFc(testing::ElementsAre(0x30, 2, 0), _)).Times(2);
    receiver.ProcessFrame(MakeMessage({ 0x10, 0x16, 1, 2, 3, 4, 5, 6 }));
    receiver.ProcessFrame(MakeMessage({ 0x21, 7, 8, 9, 10, 11, 12, 13 }));
    receiver.ProcessFrame(MakeMessage({ 0x22, 14, 15, 16, 17, 18, 19, 20 }));

    EXPECT_CALL(mocks, OnPduReady(_));
    receiver.ProcessFrame(MakeMessage({ 0x23, 21, 22 }));
}
