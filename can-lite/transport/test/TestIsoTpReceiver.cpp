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
        MOCK_METHOD(void, SendFc, (const hal::Can::Message&, const infra::Function<void()>&));
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
            [this](const hal::Can::Message& f, const infra::Function<void()>& d)
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

TEST_F(IsoTpReceiverTest, Receive_SingleFrame_ZeroLength_Aborts)
{
    auto msg = MakeMessage({ 0x00 });

    EXPECT_CALL(mocks, OnAbort(AbortReason::unexpectedFrame));
    receiver.ProcessFrame(msg);
}

TEST_F(IsoTpReceiverTest, Receive_FirstFrame_SendsFC)
{
    auto msg = MakeMessage({ 0x10, 0x08, 1, 2, 3, 4, 5, 6 });

    EXPECT_CALL(mocks, SendFc(_, _))
        .WillOnce(Invoke([](const hal::Can::Message& fc, const infra::Function<void()>&)
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
        .WillOnce(Invoke([](const hal::Can::Message& fc, const infra::Function<void()>&)
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
