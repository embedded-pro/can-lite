#include "can-lite/drivers/implementation/VirtualBusPort.hpp"

#if defined(__linux__) || defined(_WIN32)

#include "can-lite/drivers/codec/SlcanCommandDecoder.hpp"

namespace services
{
    namespace
    {
        infra::ConstByteRange BytesOf(infra::BoundedConstString text)
        {
            return { reinterpret_cast<const uint8_t*>(text.data()), reinterpret_cast<const uint8_t*>(text.data() + text.size()) };
        }
    }

    VirtualBusPort::VirtualBusPort(uint16_t hubPort)
        : hubPort(hubPort)
    {}

    VirtualBusPort::~VirtualBusPort()
    {
        VirtualBusPort::Close();
    }

    SerialPortOpenResult VirtualBusPort::Open(infra::BoundedConstString bus)
    {
        using enum SerialPortOpenResult;

        Close();

        const auto join = VirtualBusJoin(bus);
        if (join.empty())
            return notFound;

        socket = loopback::Connect(hubPort);
        if (socket == loopback::invalidSocket)
            return notFound;

        if (!Write(join))
        {
            Close();
            return failed;
        }

        return opened;
    }

    void VirtualBusPort::Close()
    {
        loopback::Close(socket);
        socket = loopback::invalidSocket;
    }

    bool VirtualBusPort::IsOpen() const
    {
        return socket != loopback::invalidSocket;
    }

    bool VirtualBusPort::Write(infra::BoundedConstString bytes)
    {
        return IsOpen() && loopback::Send(socket, BytesOf(bytes));
    }

    std::optional<std::size_t> VirtualBusPort::Read(infra::ByteRange buffer)
    {
        if (!IsOpen())
            return std::nullopt;

        return loopback::Receive(socket, buffer);
    }

    intptr_t VirtualBusPort::Handle() const
    {
#if defined(__linux__)
        return socket;
#else
        return -1;
#endif
    }

    void VirtualBusPort::EnumeratePorts(const infra::Function<void(infra::BoundedConstString)>& callback) const
    {
        const auto probe = loopback::Connect(hubPort);
        if (probe == loopback::invalidSocket)
            return;

        loopback::Close(probe);
        callback(defaultBus);
    }
}

#endif
