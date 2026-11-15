#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace services;

    class TestLoopbackSocket
        : public testing::Test
    {
    public:
        uint16_t ListenOnFreePort()
        {
            listener = loopback::Listen(0);
            EXPECT_NE(listener, loopback::invalidSocket);
            return loopback::LocalPort(listener).value_or(0);
        }

        void CloseFromTheListeningSide(uint16_t port)
        {
            const auto client = loopback::Connect(port);
            ASSERT_NE(client, loopback::invalidSocket);
            const auto accepted = loopback::Accept(listener);
            ASSERT_NE(accepted, loopback::invalidSocket);

            loopback::Close(accepted);
            loopback::Close(client);
            loopback::Close(listener);
            listener = loopback::invalidSocket;
        }

        void TearDown() override
        {
            loopback::Close(listener);
        }

        intptr_t listener{ loopback::invalidSocket };
    };
}

TEST_F(TestLoopbackSocket, a_port_whose_listener_closed_its_connections_first_can_be_listened_on_again)
{
    const auto port = ListenOnFreePort();
    CloseFromTheListeningSide(port);

    listener = loopback::Listen(port);
    EXPECT_NE(listener, loopback::invalidSocket);
}

TEST_F(TestLoopbackSocket, a_port_with_a_listener_cannot_be_listened_on_twice)
{
    const auto port = ListenOnFreePort();

    const auto second = loopback::Listen(port);
    EXPECT_EQ(second, loopback::invalidSocket);
    loopback::Close(second);
}
