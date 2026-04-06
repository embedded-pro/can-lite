#include "can-lite/transport/iso-tp/IsoTpChannelImpl.hpp"
#include "can-lite/transport/iso-tp/IsoTpFrameCodec.hpp"
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
    using TestChannel = IsoTpChannelImpl::WithStorage<TestPduSize>;

    constexpr uint32_t dataId = 0x600u;
    constexpr uint32_t fcId = 0x601u;

    struct MockCallbacks
    {
        MOCK_METHOD(void, RawSend, (uint32_t canId, const hal::Can::Message&, const infra::Function<void()>&));
        MOCK_METHOD(void, OnPduReady, (uint32_t did, infra::ConstByteRange));
        MOCK_METHOD(void, OnAbort, (uint32_t did, AbortReason));
    };
}

class IsoTpChannelTest
    : public testing::Test
    , public infra::ClockFixture
{
protected:
    StrictMock<MockCallbacks> mocks;
    TestChannel channel;

    void Configure()
    {
        channel.Configure(dataId, fcId, [this](uint32_t cid, const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                mocks.RawSend(cid, f, d);
            },
            [this](uint32_t did, infra::ConstByteRange pdu)
            {
                mocks.OnPduReady(did, pdu);
            },
            [this](uint32_t did, AbortReason r)
            {
                mocks.OnAbort(did, r);
            });
    }
};

TEST_F(IsoTpChannelTest, DefaultConstruct_NotOccupied)
{
    EXPECT_FALSE(channel.IsOccupied());
}

TEST_F(IsoTpChannelTest, Configure_SetsOccupied_WithCorrectIds)
{
    Configure();

    EXPECT_TRUE(channel.IsOccupied());
    EXPECT_EQ(channel.DataId(), dataId);
    EXPECT_EQ(channel.FcId(), fcId);
}

TEST_F(IsoTpChannelTest, Release_ClearsOccupied)
{
    Configure();
    channel.Release();

    EXPECT_FALSE(channel.IsOccupied());
}

TEST_F(IsoTpChannelTest, SendPdu_SingleFrame_CallsRawSendOnDataId)
{
    Configure();

    uint8_t pdu[] = { 0x01u, 0x02u, 0x03u };
    bool done = false;

    EXPECT_CALL(mocks, RawSend(dataId, _, _))
        .WillOnce(Invoke([](uint32_t, const hal::Can::Message& f, const infra::Function<void()>& d)
            {
                EXPECT_EQ(f[0], 0x03u);
                d();
            }));

    ASSERT_TRUE(channel.SendPdu(infra::MakeRange(pdu), [&done]
        {
            done = true;
        }));
    EXPECT_TRUE(done);
}

TEST_F(IsoTpChannelTest, SendPdu_WhenBusy_ReturnsFalse)
{
    Configure();

    uint8_t pdu[] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };

    EXPECT_CALL(mocks, RawSend(dataId, _, _));

    ASSERT_TRUE(channel.SendPdu(infra::MakeRange(pdu), [] {}));
    EXPECT_FALSE(channel.SendPdu(infra::MakeRange(pdu), [] {}));
}

TEST_F(IsoTpChannelTest, ProcessFrame_DataFrameRouted_ToReceiver)
{
    Configure();

    auto sf = MakeMessage({ 0x03u, 0xAAu, 0xBBu, 0xCCu });

    EXPECT_CALL(mocks, OnPduReady(dataId, _))
        .WillOnce(Invoke([](uint32_t, infra::ConstByteRange pdu)
            {
                ASSERT_EQ(pdu.size(), 3u);
                EXPECT_EQ(pdu[0], 0xAAu);
                EXPECT_EQ(pdu[1], 0xBBu);
                EXPECT_EQ(pdu[2], 0xCCu);
            }));

    EXPECT_TRUE(channel.ProcessFrame(dataId, sf));
}

TEST_F(IsoTpChannelTest, ProcessFrame_FcFrameRouted_ToSender)
{
    Configure();

    uint8_t pdu[] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
    bool done = false;

    {
        testing::InSequence seq;

        EXPECT_CALL(mocks, RawSend(dataId, _, _))
            .WillOnce(Invoke([](uint32_t, const hal::Can::Message&, const infra::Function<void()>& d)
                {
                    d();
                }));

        EXPECT_CALL(mocks, RawSend(dataId, _, _))
            .WillOnce(Invoke([](uint32_t, const hal::Can::Message& f, const infra::Function<void()>& d)
                {
                    EXPECT_EQ(f[0] & 0xF0u, 0x20u);
                    EXPECT_EQ(f[0] & 0x0Fu, 0x01u);
                    d();
                }));
    }

    ASSERT_TRUE(channel.SendPdu(infra::MakeRange(pdu), [&done]
        {
            done = true;
        }));

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0u, fc);

    EXPECT_TRUE(channel.ProcessFrame(fcId, fc));
    EXPECT_TRUE(done);
}

TEST_F(IsoTpChannelTest, ProcessFrame_FlowControlOnDataId_ReturnsFalse)
{
    Configure();

    hal::Can::Message fc;
    IsoTpFrameCodec::EncodeFlowControl(FlowStatus::continueToSend, 0u, 0u, fc);

    EXPECT_FALSE(channel.ProcessFrame(dataId, fc));
}

TEST_F(IsoTpChannelTest, ProcessFrame_UnrelatedId_ReturnsFalse)
{
    Configure();

    auto sf = MakeMessage({ 0x01u, 0xABu });

    EXPECT_FALSE(channel.ProcessFrame(0x999u, sf));
}

TEST_F(IsoTpChannelTest, IsSenderIdle_Initially_ReturnsTrue)
{
    Configure();
    EXPECT_TRUE(channel.IsSenderIdle());
}

TEST_F(IsoTpChannelTest, IsReceiverIdle_Initially_ReturnsTrue)
{
    Configure();
    EXPECT_TRUE(channel.IsReceiverIdle());
}

TEST_F(IsoTpChannelTest, IsSenderIdle_AfterSendStart_ReturnsFalse)
{
    Configure();

    uint8_t pdu[] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };

    EXPECT_CALL(mocks, RawSend(dataId, _, _));

    ASSERT_TRUE(channel.SendPdu(infra::MakeRange(pdu), [] {}));
    EXPECT_FALSE(channel.IsSenderIdle());
}
