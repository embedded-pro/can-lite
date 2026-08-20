#include "support/VirtualCan.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace integration;

    TEST(VirtualCanTest, ConnectedPeersDeliverToEachOther)
    {
        VirtualCan a;
        VirtualCan b;
        a.ConnectTo(b);

        bool bReceived = false;
        b.ReceiveData([&bReceived](hal::Can::Id, const hal::Can::Message&)
            {
                bReceived = true;
            });

        a.SendData(hal::Can::Id::Create29BitId(0), hal::Can::Message{}, [](bool) {});

        EXPECT_TRUE(bReceived);
    }

    TEST(VirtualCanTest, DisconnectSeversBothDirections)
    {
        VirtualCan a;
        VirtualCan b;
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

        EXPECT_FALSE(aReceived);
        EXPECT_FALSE(bReceived);
    }

    TEST(VirtualCanTest, DisconnectOnUnconnectedCanIsNoop)
    {
        VirtualCan a;

        a.Disconnect();
    }
}
