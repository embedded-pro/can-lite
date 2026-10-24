#include "can-lite/drivers/implementation/SlcanAdapter.hpp"
#include "can-lite/drivers/test/CanBusAdapterMock.hpp"
#include "can-lite/drivers/test/SerialPortMock.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <initializer_list>
#include <optional>

namespace
{
    using namespace services;
    using testing::_;
    using testing::ElementsAre;
    using testing::InSequence;
    using testing::Return;
    using testing::ReturnPointee;
    using testing::StrictMock;

    MATCHER_P(IsText, expected, "")
    {
        return arg == infra::BoundedConstString(expected);
    }

    CanFrame Bytes(std::initializer_list<uint8_t> bytes)
    {
        CanFrame frame;
        for (const auto byte : bytes)
            frame.push_back(byte);
        return frame;
    }

    class TestSlcanAdapter
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        TestSlcanAdapter()
        {
            EXPECT_CALL(port, IsOpen()).WillRepeatedly(ReturnPointee(&open));
            adapter.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
                {
                    received.Call(id, data);
                });
        }

        ~TestSlcanAdapter() override
        {
            if (!open)
                return;

            EXPECT_CALL(port, Write(IsText("C\r"))).WillOnce(Return(true));
            EXPECT_CALL(port, Close()).WillOnce([this]()
                {
                    open = false;
                });
        }

        void Connect()
        {
            EXPECT_CALL(port, Open(IsText("/dev/ttyACM0"))).WillOnce([this](infra::BoundedConstString)
                {
                    open = true;
                    return SerialPortOpenResult::opened;
                });
            EXPECT_CALL(port, Write(IsText("C\rS8\rO\r"))).WillOnce(Return(true));
            EXPECT_CALL(observer, OnConnectionChanged(true));
            ASSERT_TRUE(adapter.Connect("/dev/ttyACM0", 1000000));
        }

        void ExpectRead(infra::BoundedConstString bytes)
        {
            EXPECT_CALL(port, Read(_)).WillOnce([bytes](infra::ByteRange buffer) -> std::optional<std::size_t>
                {
                    std::copy(bytes.begin(), bytes.end(), buffer.begin());
                    return bytes.size();
                });
        }

        void Receive(infra::BoundedConstString bytes)
        {
            ExpectRead(bytes);
            adapter.ProcessReadEvent();
        }

        void ExpectOpenFailure(SerialPortOpenResult result, const char* message)
        {
            EXPECT_CALL(port, Open(_)).WillOnce(Return(result));
            EXPECT_CALL(observer, OnError(IsText(message)));
            EXPECT_FALSE(adapter.Connect("/dev/ttyACM0", 500000));
        }

        bool open{};
        StrictMock<SerialPortMock> port;
        SlcanAdapter adapter{ port };
        StrictMock<CanBusAdapterObserverMock> observer{ adapter };
        StrictMock<testing::MockFunction<void(hal::Can::Id, const hal::Can::Message&)>> received;
        StrictMock<testing::MockFunction<void(bool)>> completed;
    };
}

TEST_F(TestSlcanAdapter, connecting_closes_selects_the_bitrate_and_opens_the_channel)
{
    Connect();

    EXPECT_TRUE(adapter.IsConnected());
}

TEST_F(TestSlcanAdapter, rejects_a_bitrate_slcan_cannot_select)
{
    EXPECT_CALL(observer, OnError(_));

    EXPECT_FALSE(adapter.Connect("/dev/ttyACM0", 83333));
}

TEST_F(TestSlcanAdapter, explains_why_the_port_could_not_be_opened)
{
    ExpectOpenFailure(SerialPortOpenResult::notFound, "Serial port not found");
    ExpectOpenFailure(SerialPortOpenResult::accessDenied, "Access to the serial port was denied; on Linux, add the user to the dialout group");
    ExpectOpenFailure(SerialPortOpenResult::inUse, "The serial port is in use by another program");
    ExpectOpenFailure(SerialPortOpenResult::failed, "The serial port could not be opened");
}

TEST_F(TestSlcanAdapter, closes_the_port_when_the_adapter_cannot_be_configured)
{
    EXPECT_CALL(port, Open(_)).WillOnce([this](infra::BoundedConstString)
        {
            open = true;
            return SerialPortOpenResult::opened;
        });
    EXPECT_CALL(port, Write(_)).WillOnce(Return(false));
    EXPECT_CALL(port, Close()).WillOnce([this]()
        {
            open = false;
        });
    EXPECT_CALL(observer, OnError(IsText("Failed to configure the slcan adapter")));

    EXPECT_FALSE(adapter.Connect("/dev/ttyACM0", 1000000));
}

TEST_F(TestSlcanAdapter, delivers_received_extended_frames)
{
    Connect();

    EXPECT_CALL(observer, OnFrameLog(false, 0x123u, ElementsAre(0xab, 0xcd)));
    EXPECT_CALL(received, Call(hal::Can::Id::Create29BitId(0x123), ElementsAre(0xab, 0xcd)));
    Receive("\rT000001232ABCD\r");
}

TEST_F(TestSlcanAdapter, ignores_standard_and_remote_frames)
{
    Connect();

    Receive("\rt1230\rR000001230\r");
}

TEST_F(TestSlcanAdapter, ignores_the_answer_to_the_initial_close_but_reports_later_rejections)
{
    Connect();

    Receive("\a\r\r");

    EXPECT_CALL(observer, OnError(IsText("The slcan adapter rejected a command")));
    Receive("\a");
}

TEST_F(TestSlcanAdapter, keeps_reading_while_the_port_fills_the_buffer)
{
    Connect();
    static constexpr std::array<char, 128> acknowledgements{ []
        {
            std::array<char, 128> bytes{};
            bytes.fill('\r');
            return bytes;
        }() };

    {
        InSequence sequence;
        ExpectRead(infra::BoundedConstString(acknowledgements.data(), acknowledgements.size()));
        ExpectRead("T000001230\r");
    }
    EXPECT_CALL(observer, OnFrameLog(false, 0x123u, _));
    EXPECT_CALL(received, Call(hal::Can::Id::Create29BitId(0x123), _));
    adapter.ProcessReadEvent();
}

TEST_F(TestSlcanAdapter, reports_an_unplugged_adapter)
{
    Connect();

    InSequence sequence;
    EXPECT_CALL(port, Read(_)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(port, Close()).WillOnce([this]()
        {
            open = false;
        });
    EXPECT_CALL(observer, OnError(IsText("The slcan adapter was disconnected")));
    EXPECT_CALL(observer, OnConnectionChanged(false));
    adapter.ProcessReadEvent();

    EXPECT_FALSE(adapter.IsConnected());
}

TEST_F(TestSlcanAdapter, sends_extended_frames_and_completes_later)
{
    Connect();
    const auto data = Bytes({ 0x01, 0x02 });

    EXPECT_CALL(port, Write(IsText("T0000004220102\r"))).WillOnce(Return(true));
    EXPECT_CALL(observer, OnFrameLog(true, 0x42u, ElementsAre(0x01, 0x02)));
    adapter.SendData(hal::Can::Id::Create29BitId(0x42), data, [this](bool success)
        {
            completed.Call(success);
        });

    EXPECT_CALL(completed, Call(true));
    ExecuteAllActions();
}

TEST_F(TestSlcanAdapter, reports_a_failed_send)
{
    Connect();

    EXPECT_CALL(port, Write(_)).WillOnce(Return(false));
    EXPECT_CALL(observer, OnError(IsText("Failed to send CAN frame via slcan")));
    adapter.SendData(hal::Can::Id::Create29BitId(0x42), hal::Can::Message{}, [this](bool success)
        {
            completed.Call(success);
        });

    EXPECT_CALL(completed, Call(false));
    ExecuteAllActions();
}

TEST_F(TestSlcanAdapter, refuses_standard_identifiers)
{
    Connect();

    adapter.SendData(hal::Can::Id::Create11BitId(0x42), hal::Can::Message{}, [this](bool success)
        {
            completed.Call(success);
        });

    EXPECT_CALL(completed, Call(false));
    ExecuteAllActions();
}

TEST_F(TestSlcanAdapter, refuses_to_send_while_disconnected)
{
    adapter.SendData(hal::Can::Id::Create29BitId(0x42), hal::Can::Message{}, [this](bool success)
        {
            completed.Call(success);
        });

    EXPECT_CALL(completed, Call(false));
    ExecuteAllActions();
}

TEST_F(TestSlcanAdapter, disconnecting_closes_the_channel)
{
    Connect();

    EXPECT_CALL(port, Write(IsText("C\r"))).WillOnce(Return(true));
    EXPECT_CALL(port, Close()).WillOnce([this]()
        {
            open = false;
        });
    EXPECT_CALL(observer, OnConnectionChanged(false));
    adapter.Disconnect();

    EXPECT_FALSE(adapter.IsConnected());
}

TEST_F(TestSlcanAdapter, lists_serial_ports_as_interfaces)
{
    StrictMock<testing::MockFunction<void(infra::BoundedConstString)>> interfaces;

    EXPECT_CALL(port, EnumeratePorts(_)).WillOnce([](const infra::Function<void(infra::BoundedConstString)>& callback)
        {
            callback("/dev/ttyACM0");
        });
    EXPECT_CALL(interfaces, Call(IsText("/dev/ttyACM0")));
    adapter.EnumerateInterfaces([&interfaces](infra::BoundedConstString name)
        {
            interfaces.Call(name);
        });
}

TEST_F(TestSlcanAdapter, needs_no_driver)
{
    StrictMock<testing::MockFunction<void(infra::BoundedConstString)>> missing;

    adapter.EnumerateMissingDrivers([&missing](infra::BoundedConstString reason)
        {
            missing.Call(reason);
        });
}

TEST_F(TestSlcanAdapter, exposes_the_port_handle_for_polling)
{
    EXPECT_CALL(port, Handle()).WillOnce(Return(7));

    EXPECT_EQ(7, adapter.FileDescriptor());
}
