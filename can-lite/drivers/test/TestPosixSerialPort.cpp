#include "can-lite/drivers/implementation/PosixSerialPort.hpp"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <poll.h>
#include <thread>
#include <unistd.h>

namespace
{
    using namespace services;

    constexpr int timeoutMs{ 2000 };

    class TestPosixSerialPort
        : public testing::Test
    {
    public:
        TestPosixSerialPort()
            : master(posix_openpt(O_RDWR | O_NOCTTY))
        {
            if (master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0)
                devicePath = ptsname(master);
        }

        ~TestPosixSerialPort() override
        {
            CloseMaster();
        }

        void CloseMaster()
        {
            if (master >= 0)
                ::close(master);
            master = -1;
        }

        infra::BoundedConstString Device() const
        {
            return infra::BoundedConstString(devicePath, devicePath != nullptr ? std::strlen(devicePath) : 0);
        }

        static bool WaitReadable(int descriptor)
        {
            pollfd ready{ descriptor, POLLIN, 0 };
            return ::poll(&ready, 1, timeoutMs) > 0;
        }

        int master;
        const char* devicePath{};
        PosixSerialPort port;
    };
}

TEST_F(TestPosixSerialPort, opens_an_existing_terminal)
{
    ASSERT_FALSE(Device().empty());

    EXPECT_EQ(SerialPortOpenResult::opened, port.Open(Device()));
    EXPECT_TRUE(port.IsOpen());
    EXPECT_GE(port.Handle(), 0);

    port.Close();
    EXPECT_FALSE(port.IsOpen());
    EXPECT_EQ(-1, port.Handle());
}

TEST_F(TestPosixSerialPort, reports_a_missing_device)
{
    EXPECT_EQ(SerialPortOpenResult::notFound, port.Open("/dev/can-lite-no-such-port"));
    EXPECT_EQ(SerialPortOpenResult::notFound, port.Open(""));
    EXPECT_FALSE(port.IsOpen());
}

TEST_F(TestPosixSerialPort, reports_a_device_held_by_another_port)
{
    PosixSerialPort other;
    ASSERT_EQ(SerialPortOpenResult::opened, other.Open(Device()));

    EXPECT_EQ(SerialPortOpenResult::inUse, port.Open(Device()));
}

TEST_F(TestPosixSerialPort, writes_raw_bytes)
{
    ASSERT_EQ(SerialPortOpenResult::opened, port.Open(Device()));

    EXPECT_TRUE(port.Write("C\rS8\r"));

    ASSERT_TRUE(WaitReadable(master));
    std::array<char, 16> buffer{};
    EXPECT_EQ(5, ::read(master, buffer.data(), buffer.size()));
    EXPECT_EQ(infra::BoundedConstString("C\rS8\r"), infra::BoundedConstString(buffer.data(), 5));
}

TEST_F(TestPosixSerialPort, reads_what_is_available_without_blocking)
{
    ASSERT_EQ(SerialPortOpenResult::opened, port.Open(Device()));
    std::array<uint8_t, 16> buffer{};

    EXPECT_EQ(std::optional<std::size_t>{ 0 }, port.Read(infra::MakeRange(buffer)));

    ASSERT_EQ(3, ::write(master, "t1\r", 3));
    ASSERT_TRUE(WaitReadable(static_cast<int>(port.Handle())));
    EXPECT_EQ(std::optional<std::size_t>{ 3 }, port.Read(infra::MakeRange(buffer)));
    EXPECT_EQ('\r', buffer[2]);
}

TEST_F(TestPosixSerialPort, reports_a_lost_device)
{
    ASSERT_EQ(SerialPortOpenResult::opened, port.Open(Device()));
    std::array<uint8_t, 16> buffer{};

    CloseMaster();

    EXPECT_FALSE(port.Read(infra::MakeRange(buffer)).has_value());
}

TEST_F(TestPosixSerialPort, refuses_io_while_closed)
{
    std::array<uint8_t, 4> buffer{};

    EXPECT_FALSE(port.Write("x"));
    EXPECT_FALSE(port.Read(infra::MakeRange(buffer)).has_value());
}

TEST_F(TestPosixSerialPort, refuses_a_write_while_the_port_has_no_room)
{
    ASSERT_EQ(port.Open(Device()), SerialPortOpenResult::opened);
    const int writer{ ::open(devicePath, O_WRONLY | O_NOCTTY | O_NONBLOCK) };
    ASSERT_GE(writer, 0);

    std::array<char, 256> filler{};
    filler.fill('x');
    while (::write(writer, filler.data(), filler.size()) > 0)
    {
    }
    ::close(writer);

    EXPECT_FALSE(port.Write("T0000000180102\r"));
}

TEST_F(TestPosixSerialPort, finishes_a_write_the_port_could_only_partly_take)
{
    ASSERT_EQ(port.Open(Device()), SerialPortOpenResult::opened);
    const int writer{ ::open(devicePath, O_WRONLY | O_NOCTTY | O_NONBLOCK) };
    ASSERT_GE(writer, 0);

    std::array<char, 256> filler{};
    filler.fill('x');
    while (::write(writer, filler.data(), filler.size()) > 0)
    {
    }
    ::close(writer);

    std::array<char, 1024> drained{};
    ASSERT_TRUE(WaitReadable(master));
    ASSERT_GT(::read(master, drained.data(), drained.size()), 0);

    std::thread drainer([this]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::array<char, 1024> sink{};
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
            while (std::chrono::steady_clock::now() < deadline)
            {
                pollfd ready{ master, POLLIN, 0 };
                if (::poll(&ready, 1, 10) > 0 && ::read(master, sink.data(), sink.size()) < 0)
                    return;
            }
        });

    std::array<char, 2048> command{};
    command.fill('y');
    const bool written{ port.Write(infra::BoundedConstString(command.data(), command.size())) };
    drainer.join();

    EXPECT_TRUE(written);
}
