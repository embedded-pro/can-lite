#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/transport/IsoTpTransportImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace services;
using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace
{
    constexpr uint16_t TestPduSize = 64u;
    constexpr uint32_t dataId = 0x600u;
    constexpr uint32_t fcId = 0x601u;
}

class IsoTpTransportImplTest
    : public testing::Test
    , public infra::ClockFixture
{
protected:
    StrictMock<hal::CanMock> canMock;
    CanFrameTransport transport{ canMock, 0x100 };
    typename IsoTpTransportImpl::WithStorage<TestPduSize> isoTp{ transport };
};

TEST_F(IsoTpTransportImplTest, RegisterReceiveChannel_Once_ReturnsTrue)
{
    EXPECT_TRUE(isoTp.RegisterReceiveChannel(dataId, fcId));
}

TEST_F(IsoTpTransportImplTest, RegisterReceiveChannel_SameDataId_ReturnsFalse)
{
    EXPECT_TRUE(isoTp.RegisterReceiveChannel(dataId, fcId));
    EXPECT_FALSE(isoTp.RegisterReceiveChannel(dataId, fcId));
}

TEST_F(IsoTpTransportImplTest, RegisterReceiveChannel_SameFcId_ReturnsFalse)
{
    EXPECT_TRUE(isoTp.RegisterReceiveChannel(0x600u, 0x601u));
    EXPECT_FALSE(isoTp.RegisterReceiveChannel(0x700u, 0x601u));
}

TEST_F(IsoTpTransportImplTest, RegisterReceiveChannel_AllChannelsFull_ReturnsFalse)
{
    EXPECT_TRUE(isoTp.RegisterReceiveChannel(0x600u, 0x601u));
    EXPECT_TRUE(isoTp.RegisterReceiveChannel(0x602u, 0x603u));
    EXPECT_TRUE(isoTp.RegisterReceiveChannel(0x604u, 0x605u));
    EXPECT_TRUE(isoTp.RegisterReceiveChannel(0x606u, 0x607u));
    EXPECT_FALSE(isoTp.RegisterReceiveChannel(0x608u, 0x609u));
}

TEST_F(IsoTpTransportImplTest, ProcessFrame_UnregisteredChannel_ReturnsFalse)
{
    hal::Can::Message sf;
    sf.push_back(0x01u);
    sf.push_back(0xABu);

    EXPECT_FALSE(isoTp.ProcessFrame(dataId, sf));
}

TEST_F(IsoTpTransportImplTest, ProcessFrame_RegisteredChannel_ReturnsTrue)
{
    isoTp.RegisterReceiveChannel(dataId, fcId);
    isoTp.SetOnPduReceived([](uint32_t, infra::ConstByteRange) {});

    hal::Can::Message sf;
    sf.push_back(0x01u);
    sf.push_back(0xAAu);

    EXPECT_TRUE(isoTp.ProcessFrame(dataId, sf));
}

TEST_F(IsoTpTransportImplTest, SetOnPduReceived_CalledOnSingleFrame)
{
    bool called = false;

    isoTp.RegisterReceiveChannel(dataId, fcId);
    isoTp.SetOnPduReceived([&](uint32_t receivedDataId, infra::ConstByteRange pdu)
        {
            called = true;
            EXPECT_EQ(receivedDataId, dataId);
            ASSERT_EQ(pdu.size(), 1u);
            EXPECT_EQ(pdu[0], 0xABu);
        });

    hal::Can::Message sf;
    sf.push_back(0x01u);
    sf.push_back(0xABu);

    EXPECT_TRUE(isoTp.ProcessFrame(dataId, sf));
    EXPECT_TRUE(called);
}

TEST_F(IsoTpTransportImplTest, SendPdu_SingleFrame_CallsSendData)
{
    uint8_t pduData[] = { 0xDEu, 0xADu };
    bool doneCalled = false;

    EXPECT_CALL(canMock, SendData(_, _, _))
        .WillOnce(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
            {
                cb(true);
            }));

    EXPECT_TRUE(isoTp.SendPdu(dataId, fcId, infra::MakeRange(pduData), [&doneCalled]
        {
            doneCalled = true;
        }));
    EXPECT_TRUE(doneCalled);
}

TEST_F(IsoTpTransportImplTest, SendPdu_ExistingChannel_ReusesSameChannel)
{
    // RegisterReceiveChannel configures the channel for dataId
    EXPECT_TRUE(isoTp.RegisterReceiveChannel(dataId, fcId));

    uint8_t pduData[] = { 0x01u, 0x02u };

    EXPECT_CALL(canMock, SendData(_, _, _))
        .WillOnce(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
            {
                cb(true);
            }));

    // SendPdu with the same dataId finds the existing channel (no new allocation needed)
    bool doneCalled = false;
    EXPECT_TRUE(isoTp.SendPdu(dataId, fcId, infra::MakeRange(pduData), [&doneCalled]
        {
            doneCalled = true;
        }));
    EXPECT_TRUE(doneCalled);
}

TEST_F(IsoTpTransportImplTest, SendPdu_AllChannelsFull_ReturnsFalse)
{
    // Start 4 multi-frame sends on distinct channels — they stay in waitingForFc state
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    // Only the first SendPdu fires SendData immediately; the rest are queued by the transport
    EXPECT_CALL(canMock, SendData(_, _, _)).Times(1);

    EXPECT_TRUE(isoTp.SendPdu(0x600u, 0x601u, infra::MakeRange(pdu), [] {}));
    EXPECT_TRUE(isoTp.SendPdu(0x602u, 0x603u, infra::MakeRange(pdu), [] {}));
    EXPECT_TRUE(isoTp.SendPdu(0x604u, 0x605u, infra::MakeRange(pdu), [] {}));
    EXPECT_TRUE(isoTp.SendPdu(0x606u, 0x607u, infra::MakeRange(pdu), [] {}));

    uint8_t pdu2[] = { 0xABu };
    EXPECT_FALSE(isoTp.SendPdu(0x608u, 0x609u, infra::MakeRange(pdu2), [] {}));
}

TEST_F(IsoTpTransportImplTest, ReleaseChannel_FreesOccupiedChannel)
{
    EXPECT_TRUE(isoTp.RegisterReceiveChannel(dataId, fcId));
    ASSERT_FALSE(isoTp.RegisterReceiveChannel(dataId, fcId));

    isoTp.ReleaseChannel(dataId);

    EXPECT_TRUE(isoTp.RegisterReceiveChannel(dataId, fcId));
}

TEST_F(IsoTpTransportImplTest, ReleaseChannel_UnknownDataId_IsNoOp)
{
    isoTp.ReleaseChannel(dataId);

    EXPECT_TRUE(isoTp.RegisterReceiveChannel(dataId, fcId));
}

TEST_F(IsoTpTransportImplTest, SetOnAbort_CalledWhenChannelAborts)
{
    struct AbortInfo
    {
        bool aborted = false;
        uint32_t dataId = 0;
        iso_tp::AbortReason reason{};
    } info;

    isoTp.RegisterReceiveChannel(dataId, fcId);
    isoTp.SetOnAbort([&info](uint32_t did, iso_tp::AbortReason reason)
        {
            info.aborted = true;
            info.dataId = did;
            info.reason = reason;
        });

    EXPECT_CALL(canMock, SendData(_, _, _))
        .WillOnce(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
            {
                cb(true);
            }));

    hal::Can::Message ff;
    for (auto b : { 0x10u, 0x08u, 1u, 2u, 3u, 4u, 5u, 6u })
        ff.push_back(static_cast<uint8_t>(b));
    isoTp.ProcessFrame(dataId, ff);

    hal::Can::Message badCf;
    for (auto b : { 0x22u, 7u, 8u })
        badCf.push_back(static_cast<uint8_t>(b));
    isoTp.ProcessFrame(dataId, badCf);

    EXPECT_TRUE(info.aborted);
    EXPECT_EQ(info.dataId, dataId);
    EXPECT_EQ(info.reason, iso_tp::AbortReason::unexpectedFrame);
}

TEST_F(IsoTpTransportImplTest, SendPdu_NewChannel_ThenReceivesPdu_DispatchesViaCallback)
{
    uint8_t pduData[] = { 0xAAu };
    EXPECT_CALL(canMock, SendData(_, _, _))
        .WillOnce(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
            {
                cb(true);
            }));
    EXPECT_TRUE(isoTp.SendPdu(dataId, fcId, infra::MakeRange(pduData), [] {}));

    bool called = false;
    isoTp.SetOnPduReceived([&](uint32_t receivedDataId, infra::ConstByteRange pdu)
        {
            called = true;
            EXPECT_EQ(receivedDataId, dataId);
            ASSERT_EQ(pdu.size(), 1u);
            EXPECT_EQ(pdu[0], 0xABu);
        });

    hal::Can::Message sf;
    sf.push_back(0x01u);
    sf.push_back(0xABu);
    EXPECT_TRUE(isoTp.ProcessFrame(dataId, sf));
    EXPECT_TRUE(called);
}

TEST_F(IsoTpTransportImplTest, SendPdu_NewChannel_AbortPropagatesViaCallback)
{
    uint8_t pdu[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    EXPECT_CALL(canMock, SendData(_, _, _))
        .WillOnce(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
            {
                cb(true);
            }));
    EXPECT_TRUE(isoTp.SendPdu(dataId, fcId, infra::MakeRange(pdu), [] {}));

    struct AbortInfo
    {
        bool aborted = false;
        uint32_t dataId = 0;
        iso_tp::AbortReason reason{};
    } info;
    isoTp.SetOnAbort([&info](uint32_t did, iso_tp::AbortReason reason)
        {
            info.aborted = true;
            info.dataId = did;
            info.reason = reason;
        });

    hal::Can::Message overflowFc;
    overflowFc.push_back(0x32u);
    overflowFc.push_back(0x00u);
    overflowFc.push_back(0x00u);
    EXPECT_TRUE(isoTp.ProcessFrame(fcId, overflowFc));

    EXPECT_TRUE(info.aborted);
    EXPECT_EQ(info.dataId, dataId);
    EXPECT_EQ(info.reason, iso_tp::AbortReason::overflow);
}

TEST_F(IsoTpTransportImplTest, SendPdu_DispatchesPduToCallback)
{
    bool callbackInvoked = false;
    uint32_t receivedDataId = 0;

    isoTp.RegisterReceiveChannel(dataId, fcId);
    isoTp.SetOnPduReceived([&](uint32_t did, infra::ConstByteRange pdu)
        {
            callbackInvoked = true;
            receivedDataId = did;
            ASSERT_EQ(pdu.size(), 1u);
            EXPECT_EQ(pdu[0], 0xABu);
        });

    hal::Can::Message sf;
    sf.push_back(0x01u);
    sf.push_back(0xABu);

    EXPECT_TRUE(isoTp.ProcessFrame(dataId, sf));
    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(receivedDataId, dataId);
}
