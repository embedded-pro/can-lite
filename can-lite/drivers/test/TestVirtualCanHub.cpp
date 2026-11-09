#include "can-lite/drivers/hub/VirtualCanHub.hpp"
#include "can-lite/drivers/implementation/SlcanAdapter.hpp"
#include "can-lite/drivers/implementation/VirtualBusPort.hpp"
#include "can-lite/drivers/test/CanBusAdapterMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <optional>

namespace
{
    using namespace services;
    using testing::_;
    using testing::ElementsAre;
    using testing::StrictMock;

    MATCHER_P(IsText, expected, "")
    {
        return arg == infra::BoundedConstString(expected);
    }

    class TestVirtualCanHub
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        TestVirtualCanHub()
        {
            receiver.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
                {
                    receivedId = id.Get29BitId();
                    received = data;
                });
        }

        template<class Condition>
        bool PollUntil(Condition condition)
        {
            for (int iteration = 0; iteration != 1000 && !condition(); ++iteration)
            {
                hub.Poll();
                sender.ProcessReadEvent();
                receiver.ProcessReadEvent();
                ExecuteAllActions();
            }
            return condition();
        }

        void ConnectBoth()
        {
            EXPECT_CALL(senderObserver, OnConnectionChanged(true));
            EXPECT_CALL(receiverObserver, OnConnectionChanged(true));
            ASSERT_TRUE(sender.Connect("bench", 1000000));
            ASSERT_TRUE(receiver.Connect("bench", 1000000));
            ASSERT_TRUE(PollUntil([this]()
                {
                    return sender.IsConnected() && receiver.IsConnected();
                }));
        }

        static uint16_t Start(VirtualCanHub& hub)
        {
            EXPECT_TRUE(hub.Listen(0));
            return hub.Port().value_or(0);
        }

        VirtualCanHub hub;
        uint16_t port{ Start(hub) };
        VirtualBusPort senderPort{ port };
        VirtualBusPort receiverPort{ port };
        SlcanAdapter sender{ senderPort };
        SlcanAdapter receiver{ receiverPort };
        StrictMock<CanBusAdapterObserverMock> senderObserver{ sender };
        StrictMock<CanBusAdapterObserverMock> receiverObserver{ receiver };
        std::optional<uint32_t> receivedId;
        CanFrame received;
    };
}

TEST_F(TestVirtualCanHub, two_adapters_on_one_bus_exchange_frames)
{
    ConnectBoth();

    CanFrame data;
    data.push_back(0xca);
    data.push_back(0xfe);
    EXPECT_CALL(senderObserver, OnFrameLog(true, 0x1234567, _));
    EXPECT_CALL(receiverObserver, OnFrameLog(false, 0x1234567, _));
    sender.SendData(hal::Can::Id::Create29BitId(0x1234567), data, [](bool success)
        {
            EXPECT_TRUE(success);
        });

    PollUntil([]()
        {
            return false;
        });
    EXPECT_FALSE(receivedId.has_value());

    ForwardTime(std::chrono::milliseconds(1));
    ASSERT_TRUE(PollUntil([this]()
        {
            return receivedId.has_value();
        }));
    EXPECT_EQ(*receivedId, 0x1234567u);
    EXPECT_THAT(received, ElementsAre(0xca, 0xfe));
}

TEST_F(TestVirtualCanHub, an_adapter_at_another_bitrate_is_refused)
{
    EXPECT_CALL(senderObserver, OnConnectionChanged(true));
    ASSERT_TRUE(sender.Connect("bench", 1000000));
    ASSERT_TRUE(PollUntil([this]()
        {
            return sender.IsConnected();
        }));

    EXPECT_CALL(receiverObserver, OnError(IsText("The slcan adapter rejected the bitrate or the open command")));
    EXPECT_CALL(receiverObserver, OnConnectionChanged(false));
    ASSERT_TRUE(receiver.Connect("bench", 500000));
    PollUntil([]()
        {
            return false;
        });
    EXPECT_FALSE(receiver.IsConnected());
}

TEST_F(TestVirtualCanHub, losing_the_hub_closes_the_adapters)
{
    ConnectBoth();

    EXPECT_CALL(senderObserver, OnError(IsText("The slcan adapter was disconnected")));
    EXPECT_CALL(receiverObserver, OnError(IsText("The slcan adapter was disconnected")));
    EXPECT_CALL(senderObserver, OnConnectionChanged(false));
    EXPECT_CALL(receiverObserver, OnConnectionChanged(false));
    hub.Stop();
    EXPECT_TRUE(PollUntil([this]()
        {
            return !sender.IsConnected() && !receiver.IsConnected();
        }));
}

TEST_F(TestVirtualCanHub, without_a_hub_the_port_does_not_open)
{
    hub.Stop();
    VirtualBusPort orphan{ port };

    EXPECT_EQ(orphan.Open("bench"), SerialPortOpenResult::notFound);
}

TEST_F(TestVirtualCanHub, the_port_lists_the_default_bus_while_a_hub_listens)
{
    std::optional<infra::BoundedConstString> listed;
    senderPort.EnumeratePorts([&listed](infra::BoundedConstString name)
        {
            listed = name;
        });

    ASSERT_TRUE(listed.has_value());
    EXPECT_EQ(*listed, infra::BoundedConstString("default"));
}
