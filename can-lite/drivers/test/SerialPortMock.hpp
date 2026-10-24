#pragma once

#include "can-lite/drivers/interface/SerialPort.hpp"
#include "gmock/gmock.h"

namespace services
{
    class SerialPortMock
        : public SerialPort
    {
    public:
        MOCK_METHOD(SerialPortOpenResult, Open, (infra::BoundedConstString path), (override));
        MOCK_METHOD(void, Close, (), (override));
        MOCK_METHOD(bool, IsOpen, (), (const, override));
        MOCK_METHOD(bool, Write, (infra::BoundedConstString bytes), (override));
        MOCK_METHOD(std::optional<std::size_t>, Read, (infra::ByteRange buffer), (override));
        MOCK_METHOD(intptr_t, Handle, (), (const, override));
        MOCK_METHOD(void, EnumeratePorts, (const infra::Function<void(infra::BoundedConstString)>& callback), (const, override));
    };
}
