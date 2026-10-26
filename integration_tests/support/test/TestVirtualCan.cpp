#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "support/VirtualCan.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace integration;

    class VirtualCanTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message message;
            for (auto byte : bytes)
                message.push_back(byte);
            return message;
        }

        VirtualCan a;
        VirtualCan b;
    };

    TEST_F(VirtualCanTest, ConnectedPeersDeliverToEachOther)
    {
        a.ConnectTo(b);

        bool bReceived = false;
        b.ReceiveData([&bReceived](hal::Can::Id, const hal::Can::Message&)
            {
                bReceived = true;
            });

        a.SendData(hal::Can::Id::Create29BitId(0), hal::Can::Message{}, [](bool) {});
        ExecuteAllActions();

        EXPECT_TRUE(bReceived);
    }

    TEST_F(VirtualCanTest, DeliveryAndCompletionHappenAfterSendDataReturns)
    {
        a.ConnectTo(b);

        bool bReceived = false;
        bool doneCalled = false;
        b.ReceiveData([&bReceived](hal::Can::Id, const hal::Can::Message&)
            {
                bReceived = true;
            });

        a.SendData(hal::Can::Id::Create29BitId(0), hal::Can::Message{}, [&doneCalled, &bReceived](bool success)
            {
                EXPECT_TRUE(bReceived);
                EXPECT_TRUE(success);
                doneCalled = true;
            });

        EXPECT_FALSE(bReceived);
        EXPECT_FALSE(doneCalled);

        ExecuteAllActions();

        EXPECT_TRUE(doneCalled);
    }

    TEST_F(VirtualCanTest, FramesAreDeliveredInTheOrderTheyWereSent)
    {
        a.ConnectTo(b);

        hal::Can::Message received;
        b.ReceiveData([&received](hal::Can::Id, const hal::Can::Message& data)
            {
                received.push_back(data[0]);
            });

        a.SendData(hal::Can::Id::Create29BitId(1), MakeMessage({ 1 }), [](bool) {});
        a.SendData(hal::Can::Id::Create29BitId(2), MakeMessage({ 2 }), [](bool) {});
        ExecuteAllActions();

        EXPECT_EQ(received, MakeMessage({ 1, 2 }));
    }

    TEST_F(VirtualCanTest, EverySentFrameIsRecorded)
    {
        a.SendData(hal::Can::Id::Create29BitId(1), MakeMessage({ 1 }), [](bool) {});
        a.SendData(hal::Can::Id::Create29BitId(2), MakeMessage({ 2 }), [](bool) {});
        ExecuteAllActions();

        ASSERT_EQ(a.SentFrames().size(), 2u);
        EXPECT_EQ(a.SentFrames()[0].id, hal::Can::Id::Create29BitId(1));
        EXPECT_EQ(a.LastSent().data, MakeMessage({ 2 }));
        EXPECT_EQ(a.sendCount, 2);
    }

    TEST_F(VirtualCanTest, DisconnectSeversBothDirections)
    {
        a.ConnectTo(b);

        bool aReceived = false;
        bool bReceived = false;
        a.ReceiveData([&aReceived](hal::Can::Id, const hal::Can::Message&)
            {
                aReceived = true;
            });
        b.ReceiveData([&bReceived](hal::Can::Id, const hal::Can::Message&)
            {
                bReceived = true;
            });

        a.Disconnect();

        a.SendData(hal::Can::Id::Create29BitId(0), hal::Can::Message{}, [](bool) {});
        b.SendData(hal::Can::Id::Create29BitId(0), hal::Can::Message{}, [](bool) {});
        ExecuteAllActions();

        EXPECT_FALSE(aReceived);
        EXPECT_FALSE(bReceived);
    }

    TEST_F(VirtualCanTest, DisconnectOnUnconnectedCanIsNoop)
    {
        a.Disconnect();

        bool doneCalled = false;
        a.SendData(hal::Can::Id::Create29BitId(0), hal::Can::Message{}, [&doneCalled](bool)
            {
                doneCalled = true;
            });
        ExecuteAllActions();

        EXPECT_EQ(a.sendCount, 1);
        EXPECT_TRUE(doneCalled);
    }
}
