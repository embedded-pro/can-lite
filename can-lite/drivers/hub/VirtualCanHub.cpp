#include "can-lite/drivers/hub/VirtualCanHub.hpp"

#if defined(__linux__) || defined(_WIN32)

namespace services
{
    namespace
    {
        constexpr std::size_t readChunkSize{ 256 };
        constexpr std::size_t maxReadChunks{ 16 };
    }

    VirtualCanHub::~VirtualCanHub()
    {
        Stop();
    }

    bool VirtualCanHub::Listen(uint16_t port)
    {
        Stop();
        listener = loopback::Listen(port);
        return IsListening();
    }

    void VirtualCanHub::Stop()
    {
        for (auto& connection : connections)
            if (connection.socket != loopback::invalidSocket)
                Drop(connection);

        loopback::Close(listener);
        listener = loopback::invalidSocket;
    }

    bool VirtualCanHub::IsListening() const
    {
        return listener != loopback::invalidSocket;
    }

    std::optional<uint16_t> VirtualCanHub::Port() const
    {
        if (!IsListening())
            return std::nullopt;

        return loopback::LocalPort(listener);
    }

    void VirtualCanHub::Poll()
    {
        if (!IsListening())
            return;

        AcceptPending();
        for (auto& connection : connections)
            if (connection.socket != loopback::invalidSocket)
                ReadFrom(connection);
    }

    void VirtualCanHub::AcceptPending()
    {
        for (auto socket = loopback::Accept(listener); socket != loopback::invalidSocket; socket = loopback::Accept(listener))
        {
            Connection* free{};
            for (auto& connection : connections)
                if (free == nullptr && connection.socket == loopback::invalidSocket)
                    free = &connection;

            if (free == nullptr || !bus.Attach(*free))
            {
                loopback::Close(socket);
                continue;
            }

            free->socket = socket;
        }
    }

    void VirtualCanHub::ReadFrom(Connection& connection)
    {
        std::array<uint8_t, readChunkSize> buffer{};
        for (std::size_t chunk = 0; chunk != maxReadChunks; ++chunk)
        {
            const auto received = loopback::Receive(connection.socket, infra::MakeRange(buffer));
            if (!received.has_value())
            {
                Drop(connection);
                return;
            }

            if (*received == 0)
                return;

            bus.Receive(connection, infra::Head(infra::MakeConstRange(buffer), *received));
        }
    }

    void VirtualCanHub::Drop(Connection& connection)
    {
        bus.Detach(connection);
        loopback::Close(connection.socket);
        connection.socket = loopback::invalidSocket;
    }

    void VirtualCanHub::Connection::Write(infra::BoundedConstString bytes)
    {
        loopback::Send(socket, { reinterpret_cast<const uint8_t*>(bytes.data()), reinterpret_cast<const uint8_t*>(bytes.data() + bytes.size()) });
    }
}

#endif
