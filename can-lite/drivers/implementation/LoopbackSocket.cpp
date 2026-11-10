#include "can-lite/drivers/implementation/LoopbackSocket.hpp"

#if defined(__linux__) || defined(_WIN32)

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace services::loopback
{
    namespace
    {
#if defined(__linux__)
        using Native = int;
        constexpr Native nativeInvalid{ -1 };

        void CloseNative(Native socket)
        {
            ::close(socket);
        }
#else
        using Native = SOCKET;
        constexpr Native nativeInvalid{ INVALID_SOCKET };

        void CloseNative(Native socket)
        {
            ::closesocket(socket);
        }

        bool StartWinsock()
        {
            static const bool started = []()
            {
                WSADATA data{};
                return WSAStartup(MAKEWORD(2, 2), &data) == 0;
            }();
            return started;
        }
#endif

        Native ToNative(intptr_t socket)
        {
            return static_cast<Native>(socket);
        }

        intptr_t FromNative(Native socket)
        {
            return socket == nativeInvalid ? invalidSocket : static_cast<intptr_t>(socket);
        }

        sockaddr_in LoopbackAddress(uint16_t port)
        {
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            return address;
        }

        Native OpenStream()
        {
#if defined(_WIN32)
            if (!StartWinsock())
                return nativeInvalid;
#endif
            return ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        }

        void DisableDelay(Native socket)
        {
            int enabled{ 1 };
            ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&enabled), sizeof(enabled));
        }

        bool Readable(Native socket)
        {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(socket, &readSet);
            timeval immediately{};
            return ::select(static_cast<int>(socket) + 1, &readSet, nullptr, nullptr, &immediately) > 0;
        }
    }

    intptr_t Connect(uint16_t port)
    {
        const auto socket = OpenStream();
        if (socket == nativeInvalid)
            return invalidSocket;

        auto address = LoopbackAddress(port);
        if (::connect(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
        {
            CloseNative(socket);
            return invalidSocket;
        }

        DisableDelay(socket);
        return FromNative(socket);
    }

    intptr_t Listen(uint16_t port)
    {
        const auto socket = OpenStream();
        if (socket == nativeInvalid)
            return invalidSocket;

        auto address = LoopbackAddress(port);
        if (::bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 || ::listen(socket, SOMAXCONN) != 0)
        {
            CloseNative(socket);
            return invalidSocket;
        }

        return FromNative(socket);
    }

    std::optional<uint16_t> LocalPort(intptr_t socket)
    {
        sockaddr_in address{};
        socklen_t length{ sizeof(address) };
        if (::getsockname(ToNative(socket), reinterpret_cast<sockaddr*>(&address), &length) != 0)
            return std::nullopt;

        return ntohs(address.sin_port);
    }

    intptr_t Accept(intptr_t listener)
    {
        if (!Readable(ToNative(listener)))
            return invalidSocket;

        const auto socket = ::accept(ToNative(listener), nullptr, nullptr);
        if (socket == nativeInvalid)
            return invalidSocket;

        DisableDelay(socket);
        return FromNative(socket);
    }

    bool Send(intptr_t socket, infra::ConstByteRange bytes)
    {
        while (!bytes.empty())
        {
            const auto sent = ::send(ToNative(socket), reinterpret_cast<const char*>(bytes.begin()), static_cast<int>(bytes.size()), 0);
            if (sent <= 0)
                return false;
            bytes.shrink_from_front_to(bytes.size() - static_cast<std::size_t>(sent));
        }
        return true;
    }

    std::optional<std::size_t> Receive(intptr_t socket, infra::ByteRange buffer)
    {
        if (!Readable(ToNative(socket)))
            return std::size_t{ 0 };

        const auto received = ::recv(ToNative(socket), reinterpret_cast<char*>(buffer.begin()), static_cast<int>(buffer.size()), 0);
        if (received <= 0)
            return std::nullopt;

        return static_cast<std::size_t>(received);
    }

    void Close(intptr_t socket)
    {
        if (socket != invalidSocket)
            CloseNative(ToNative(socket));
    }
}

#endif
