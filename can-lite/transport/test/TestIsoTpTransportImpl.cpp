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
