#pragma once

#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include "can-lite/drivers/interface/SerialPort.hpp"

#if defined(__linux__) || defined(_WIN32)

namespace services
{
    class VirtualBusPort
        : public SerialPort
    {
    public:
        static constexpr const char* defaultBus{ "default" };

        explicit VirtualBusPort(uint16_t hubPort = loopback::virtualCanHubPort);
        ~VirtualBusPort();

        SerialPortOpenResult Open(infra::BoundedConstString bus) override;
        void Close() override;
        bool IsOpen() const override;
        bool Write(infra::BoundedConstString bytes) override;
        std::optional<std::size_t> Read(infra::ByteRange buffer) override;
        intptr_t Handle() const override;
        void EnumeratePorts(const infra::Function<void(infra::BoundedConstString)>& callback) const override;

    private:
        uint16_t hubPort;
        intptr_t socket{ loopback::invalidSocket };
    };
}

#endif
