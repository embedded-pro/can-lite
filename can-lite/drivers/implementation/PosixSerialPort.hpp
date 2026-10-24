#pragma once

#include "can-lite/drivers/interface/SerialPort.hpp"

#ifdef __linux__

namespace services
{
    class PosixSerialPort
        : public SerialPort
    {
    public:
        PosixSerialPort() = default;
        ~PosixSerialPort();

        SerialPortOpenResult Open(infra::BoundedConstString path) override;
        void Close() override;
        bool IsOpen() const override;
        bool Write(infra::BoundedConstString bytes) override;
        std::optional<std::size_t> Read(infra::ByteRange buffer) override;
        intptr_t Handle() const override;
        void EnumeratePorts(const infra::Function<void(infra::BoundedConstString)>& callback) const override;

    private:
        int descriptor{ -1 };
    };
}

#endif
