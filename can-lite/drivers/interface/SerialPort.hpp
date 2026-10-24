#pragma once

#include "infra/util/BoundedString.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    enum class SerialPortOpenResult
    {
        opened,
        notFound,
        accessDenied,
        inUse,
        failed
    };

    class SerialPort
    {
    public:
        SerialPort() = default;
        SerialPort(const SerialPort&) = delete;
        SerialPort& operator=(const SerialPort&) = delete;

        virtual SerialPortOpenResult Open(infra::BoundedConstString path) = 0;
        virtual void Close() = 0;
        virtual bool IsOpen() const = 0;
        virtual bool Write(infra::BoundedConstString bytes) = 0;
        virtual std::optional<std::size_t> Read(infra::ByteRange buffer) = 0;
        virtual intptr_t Handle() const = 0;
        virtual void EnumeratePorts(const infra::Function<void(infra::BoundedConstString)>& callback) const = 0;

    protected:
        ~SerialPort() = default;
    };
}
