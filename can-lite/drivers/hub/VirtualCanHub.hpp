#pragma once

#include "can-lite/drivers/hub/VirtualCanBus.hpp"
#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include <array>
#include <cstdint>
#include <optional>

#if defined(__linux__) || defined(_WIN32)

namespace services
{
    class VirtualCanHub
    {
    public:
        VirtualCanHub() = default;
        VirtualCanHub(const VirtualCanHub&) = delete;
        VirtualCanHub& operator=(const VirtualCanHub&) = delete;
        ~VirtualCanHub();

        bool Listen(uint16_t port = loopback::virtualCanHubPort);
        void Stop();
        bool IsListening() const;
        std::optional<uint16_t> Port() const;
        void Poll();

    private:
        class Connection
            : public VirtualCanParticipant
        {
        public:
            void Write(infra::BoundedConstString bytes) override;

            intptr_t socket{ loopback::invalidSocket };
        };

        void AcceptPending();
        void ReadFrom(Connection& connection);
        void Drop(Connection& connection);

        VirtualCanBus bus;
        std::array<Connection, VirtualCanBus::maxParticipants> connections;
        intptr_t listener{ loopback::invalidSocket };
    };
}

#endif
