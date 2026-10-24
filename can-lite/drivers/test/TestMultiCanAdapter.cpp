#include "can-lite/drivers/implementation/MultiCanAdapter.hpp"
#include "can-lite/drivers/test/CanBusAdapterMock.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::InSequence;
    using testing::Return;
    using testing::StrictMock;

    MATCHER_P(IsText, expected, "")
    {
        return arg == infra::BoundedConstString(expected);
    }

    using Callback = infra::Function<void(infra::BoundedConstString)>;

    auto Reports(const char* first, const char* second = nullptr)
    {
        return [first, second](const Callback& callback)
        {
            callback(first);
            if (second != nullptr)
                callback(second);
        };
    }

    auto ReportsNothing()
    {
        return [](const Callback&) {};
    }

    class TestMultiCanAdapter
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        TestMultiCanAdapter()
        {
            EXPECT_CALL(socketCan, ReceiveData(_)).WillOnce([this](const auto& callback)
                                                      {
                                                          socketCanReceive = callback;
                                                      })
                .WillOnce(Return());
            EXPECT_CALL(slcan, ReceiveData(_)).WillOnce([this](const auto& callback)
                                                  {
                                                      slcanReceive = callback;
                                                  })
                .WillOnce(Return());

            multi.Add("", socketCan);
            multi.Add("slcan", slcan, "CANable");
            multi.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
                {
                    received.Call(id, data);
                });
        }

        void ConnectSlcan()
        {
            EXPECT_CALL(slcan, Connect(IsText("/dev/ttyACM0"), 1000000)).WillOnce(Return(true));
            ASSERT_TRUE(multi.Connect("slcan:/dev/ttyACM0", 1000000));
        }

        static void NotifyConnection(CanBusAdapter& backend, bool connected)
        {
            backend.NotifyObservers([connected](auto& observer)
                {
                    observer.OnConnectionChanged(connected);
                });
        }

        void ListMissingDrivers()
        {
            multi.EnumerateMissingDrivers([this](infra::BoundedConstString reason)
                {
                    missing.Call(reason);
                });
        }

        StrictMock<CanBusAdapterMock> socketCan;
        StrictMock<CanBusAdapterMock> slcan;
        MultiCanAdapter::WithMaxBackends<2> multi;
        StrictMock<CanBusAdapterObserverMock> observer{ multi };
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> socketCanReceive;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> slcanReceive;
        StrictMock<testing::MockFunction<void(hal::Can::Id, const hal::Can::Message&)>> received;
        StrictMock<testing::MockFunction<void(infra::BoundedConstString)>> missing;
        StrictMock<testing::MockFunction<void(bool)>> completed;
    };
}

TEST_F(TestMultiCanAdapter, routes_a_prefixed_name_to_its_backend_without_the_prefix)
{
    ConnectSlcan();

    EXPECT_CALL(slcan, IsConnected()).WillOnce(Return(true));
    EXPECT_TRUE(multi.IsConnected());
}

TEST_F(TestMultiCanAdapter, routes_a_bare_name_to_the_backend_without_a_prefix)
{
    EXPECT_CALL(socketCan, Connect(IsText("can0"), 500000)).WillOnce(Return(true));

    EXPECT_TRUE(multi.Connect("can0", 500000));
}

TEST_F(TestMultiCanAdapter, routes_an_alias_to_its_backend)
{
    EXPECT_CALL(slcan, Connect(IsText("/dev/ttyACM1"), 250000)).WillOnce(Return(true));

    EXPECT_TRUE(multi.Connect("CANable:/dev/ttyACM1", 250000));
}

TEST_F(TestMultiCanAdapter, rejects_an_unknown_prefix_and_names_the_accepted_forms)
{
    EXPECT_CALL(observer, OnError(IsText("Unknown interface \"PCAN:1\"; use one of: <interface>, slcan:<channel>")));

    EXPECT_FALSE(multi.Connect("PCAN:1", 500000));
    EXPECT_FALSE(multi.IsConnected());
}

TEST_F(TestMultiCanAdapter, stays_disconnected_when_the_backend_fails_to_connect)
{
    EXPECT_CALL(slcan, Connect(_, _)).WillOnce(Return(false));

    EXPECT_FALSE(multi.Connect("slcan:/dev/ttyACM0", 1000000));
    EXPECT_FALSE(multi.IsConnected());
    EXPECT_EQ(-1, multi.FileDescriptor());
}

TEST_F(TestMultiCanAdapter, disconnects_the_previous_backend_before_connecting_another)
{
    ConnectSlcan();

    InSequence sequence;
    EXPECT_CALL(slcan, IsConnected()).WillOnce(Return(true));
    EXPECT_CALL(slcan, Disconnect());
    EXPECT_CALL(socketCan, Connect(IsText("can0"), 1000000)).WillOnce(Return(true));
    EXPECT_TRUE(multi.Connect("can0", 1000000));
}

TEST_F(TestMultiCanAdapter, lists_interfaces_with_their_backend_prefix)
{
    StrictMock<testing::MockFunction<void(infra::BoundedConstString)>> interfaces;

    EXPECT_CALL(socketCan, EnumerateInterfaces(_)).WillOnce(Reports("can0", "vcan0"));
    EXPECT_CALL(slcan, EnumerateInterfaces(_)).WillOnce(Reports("/dev/ttyACM0"));
    InSequence sequence;
    EXPECT_CALL(interfaces, Call(IsText("can0")));
    EXPECT_CALL(interfaces, Call(IsText("vcan0")));
    EXPECT_CALL(interfaces, Call(IsText("slcan:/dev/ttyACM0")));
    multi.EnumerateInterfaces([&interfaces](infra::BoundedConstString name)
        {
            interfaces.Call(name);
        });
}

TEST_F(TestMultiCanAdapter, forwards_events_raised_while_the_backend_connects)
{
    EXPECT_CALL(slcan, Connect(_, _)).WillOnce([this](infra::BoundedConstString, uint32_t)
        {
            NotifyConnection(slcan, true);
            return true;
        });
    EXPECT_CALL(observer, OnConnectionChanged(true));

    EXPECT_TRUE(multi.Connect("slcan:/dev/ttyACM0", 1000000));
}

TEST_F(TestMultiCanAdapter, forwards_only_the_active_backend_events_and_frames)
{
    ConnectSlcan();

    socketCan.NotifyObservers([](auto& observer)
        {
            observer.OnError("not the active backend");
        });
    socketCanReceive(hal::Can::Id::Create29BitId(0x1), hal::Can::Message{});

    EXPECT_CALL(observer, OnError(IsText("slcan failure")));
    EXPECT_CALL(observer, OnFrameLog(true, 0x123u, _));
    EXPECT_CALL(received, Call(hal::Can::Id::Create29BitId(0x1234), _));
    slcan.NotifyObservers([](auto& observer)
        {
            observer.OnError("slcan failure");
            observer.OnFrameLog(true, 0x123, CanFrame{});
        });
    slcanReceive(hal::Can::Id::Create29BitId(0x1234), hal::Can::Message{});
}

TEST_F(TestMultiCanAdapter, sends_through_the_active_backend)
{
    ConnectSlcan();

    EXPECT_CALL(slcan, SendData(hal::Can::Id::Create29BitId(0x42), _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& done)
        {
            done(true);
        });
    EXPECT_CALL(completed, Call(true));
    multi.SendData(hal::Can::Id::Create29BitId(0x42), hal::Can::Message{}, [this](bool success)
        {
            completed.Call(success);
        });
}

TEST_F(TestMultiCanAdapter, completes_a_send_without_connection_later_and_unsuccessfully)
{
    multi.SendData(hal::Can::Id::Create29BitId(0x42), hal::Can::Message{}, [this](bool success)
        {
            completed.Call(success);
        });

    EXPECT_CALL(completed, Call(false));
    ExecuteAllActions();
}

TEST_F(TestMultiCanAdapter, polls_the_active_backend)
{
    ConnectSlcan();

    EXPECT_CALL(slcan, FileDescriptor()).WillOnce(Return(7));
    EXPECT_EQ(7, multi.FileDescriptor());

    EXPECT_CALL(slcan, ProcessReadEvent());
    multi.ProcessReadEvent();
}

TEST_F(TestMultiCanAdapter, ignores_polling_without_connection)
{
    multi.ProcessReadEvent();
}

TEST_F(TestMultiCanAdapter, forgets_the_backend_when_it_reports_a_disconnect)
{
    ConnectSlcan();

    EXPECT_CALL(observer, OnConnectionChanged(false));
    NotifyConnection(slcan, false);

    EXPECT_FALSE(multi.IsConnected());
    EXPECT_EQ(-1, multi.FileDescriptor());
}

TEST_F(TestMultiCanAdapter, disconnects_the_active_backend)
{
    ConnectSlcan();

    EXPECT_CALL(slcan, Disconnect());
    multi.Disconnect();

    EXPECT_FALSE(multi.IsConnected());
}

TEST_F(TestMultiCanAdapter, reports_no_missing_driver_while_a_backend_is_usable)
{
    EXPECT_CALL(socketCan, EnumerateMissingDrivers(_)).WillOnce(ReportsNothing());
    EXPECT_CALL(socketCan, EnumerateInterfaces(_)).WillOnce(Reports("can0"));

    ListMissingDrivers();
}

TEST_F(TestMultiCanAdapter, reports_every_missing_driver_when_no_backend_is_usable)
{
    EXPECT_CALL(socketCan, EnumerateMissingDrivers(_)).WillRepeatedly(Reports("SocketCAN missing"));
    EXPECT_CALL(socketCan, EnumerateInterfaces(_)).WillOnce(ReportsNothing());
    EXPECT_CALL(slcan, EnumerateMissingDrivers(_)).WillRepeatedly(ReportsNothing());
    EXPECT_CALL(slcan, EnumerateInterfaces(_)).WillOnce(ReportsNothing());

    EXPECT_CALL(missing, Call(IsText("SocketCAN missing")));
    ListMissingDrivers();
}

TEST_F(TestMultiCanAdapter, a_backend_with_a_missing_driver_is_not_usable_even_with_interfaces)
{
    EXPECT_CALL(socketCan, EnumerateMissingDrivers(_)).WillRepeatedly(Reports("SocketCAN missing"));
    EXPECT_CALL(socketCan, EnumerateInterfaces(_)).WillOnce(Reports("can0"));
    EXPECT_CALL(slcan, EnumerateMissingDrivers(_)).WillRepeatedly(Reports("serial missing"));
    EXPECT_CALL(slcan, EnumerateInterfaces(_)).WillOnce(ReportsNothing());

    InSequence sequence;
    EXPECT_CALL(missing, Call(IsText("SocketCAN missing")));
    EXPECT_CALL(missing, Call(IsText("serial missing")));
    ListMissingDrivers();
}
