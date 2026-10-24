#ifdef __linux__

#include "can-lite/drivers/implementation/PosixSerialPort.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/file.h>
#include <termios.h>
#include <unistd.h>

namespace services
{
    namespace
    {
        constexpr std::size_t maxPathLength{ 256 };
        constexpr int maxPortIndex{ 32 };
        constexpr std::array<const char*, 2> portPrefixes{ "/dev/ttyACM", "/dev/ttyUSB" };

        SerialPortOpenResult ResultFor(int error)
        {
            switch (error)
            {
                case ENOENT:
                case ENODEV:
                case ENXIO:
                    return SerialPortOpenResult::notFound;
                case EACCES:
                case EPERM:
                    return SerialPortOpenResult::accessDenied;
                case EBUSY:
                case EWOULDBLOCK:
                    return SerialPortOpenResult::inUse;
                default:
                    return SerialPortOpenResult::failed;
            }
        }

        void ConfigureRaw(int descriptor)
        {
            termios settings{};
            if (tcgetattr(descriptor, &settings) != 0)
                return;

            cfmakeraw(&settings);
            cfsetispeed(&settings, B115200);
            cfsetospeed(&settings, B115200);
            settings.c_cflag |= CLOCAL | CREAD;
            tcsetattr(descriptor, TCSANOW, &settings);
            tcflush(descriptor, TCIOFLUSH);
        }
    }

    PosixSerialPort::~PosixSerialPort()
    {
        Close();
    }

    SerialPortOpenResult PosixSerialPort::Open(infra::BoundedConstString path)
    {
        Close();

        std::array<char, maxPathLength> terminated{};
        if (path.empty() || path.size() >= terminated.size())
            return SerialPortOpenResult::notFound;
        std::copy(path.begin(), path.end(), terminated.begin());

        descriptor = ::open(terminated.data(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
        if (descriptor < 0)
            return ResultFor(errno);

        if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0)
        {
            const auto error = errno;
            Close();
            return ResultFor(error);
        }

        ConfigureRaw(descriptor);
        return SerialPortOpenResult::opened;
    }

    void PosixSerialPort::Close()
    {
        if (descriptor >= 0)
            ::close(descriptor);
        descriptor = -1;
    }

    bool PosixSerialPort::IsOpen() const
    {
        return descriptor >= 0;
    }

    bool PosixSerialPort::Write(infra::BoundedConstString bytes)
    {
        std::size_t offset{};
        while (descriptor >= 0 && offset < bytes.size())
        {
            const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
            if (written > 0)
                offset += static_cast<std::size_t>(written);
            else if (written == 0 || errno != EINTR)
                return false;
        }

        return descriptor >= 0;
    }

    std::optional<std::size_t> PosixSerialPort::Read(infra::ByteRange buffer)
    {
        while (descriptor >= 0)
        {
            const auto received = ::read(descriptor, buffer.begin(), buffer.size());
            if (received > 0)
                return static_cast<std::size_t>(received);

            if (received < 0 && errno == EINTR)
                continue;

            if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                return 0;

            return std::nullopt;
        }

        return std::nullopt;
    }

    intptr_t PosixSerialPort::Handle() const
    {
        return descriptor;
    }

    void PosixSerialPort::EnumeratePorts(const infra::Function<void(infra::BoundedConstString)>& callback) const
    {
        for (const auto* prefix : portPrefixes)
            for (int index = 0; index < maxPortIndex; ++index)
            {
                std::array<char, maxPathLength> path{};
                const auto length = std::snprintf(path.data(), path.size(), "%s%d", prefix, index);
                if (length > 0 && ::access(path.data(), F_OK) == 0)
                    callback(infra::BoundedConstString(path.data(), static_cast<std::size_t>(length)));
            }
    }
}

#endif
