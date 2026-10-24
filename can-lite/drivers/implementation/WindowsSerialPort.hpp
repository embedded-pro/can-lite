#pragma once

#include "can-lite/drivers/interface/SerialPort.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace services
{
    class WindowsSerialPort
        : public SerialPort
    {
    public:
        WindowsSerialPort() = default;
        ~WindowsSerialPort();

        SerialPortOpenResult Open(infra::BoundedConstString path) override;
        void Close() override;
        bool IsOpen() const override;
        bool Write(infra::BoundedConstString bytes) override;
        std::optional<std::size_t> Read(infra::ByteRange buffer) override;
        intptr_t Handle() const override;
        void EnumeratePorts(const infra::Function<void(infra::BoundedConstString)>& callback) const override;

    private:
        HANDLE handle{ INVALID_HANDLE_VALUE };
    };
}

#endif
