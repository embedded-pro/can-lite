#pragma once

#include "can-lite/drivers/implementation/MultiCanAdapter.hpp"
#include "can-lite/drivers/implementation/SlcanAdapter.hpp"
#include "can-lite/drivers/implementation/VirtualBusPort.hpp"

#if defined(__linux__)
#include "can-lite/drivers/implementation/PosixSerialPort.hpp"
#include "can-lite/drivers/implementation/SocketCanAdapter.hpp"
#elif defined(_WIN32)
#include "can-lite/drivers/implementation/GsUsbAdapter.hpp"
#include "can-lite/drivers/implementation/KvaserAdapter.hpp"
#include "can-lite/drivers/implementation/PcanAdapter.hpp"
#include "can-lite/drivers/implementation/WindowsSerialPort.hpp"
#endif

#if defined(__linux__) || defined(_WIN32)

namespace services
{
    class PlatformCanAdapter
    {
    public:
        PlatformCanAdapter();

        CanBusAdapter& Adapter();

    private:
#if defined(__linux__)
        SocketCanAdapter socketCan;
        PosixSerialPort serialPort;
        SlcanAdapter slcan{ serialPort };
        VirtualBusPort virtualPort;
        SlcanAdapter virtualBus{ virtualPort };
        MultiCanAdapter::WithMaxBackends<3> adapter;
#else
        PcanAdapter pcan;
        KvaserAdapter kvaser;
        WindowsSerialPort serialPort;
        SlcanAdapter slcan{ serialPort };
        GsUsbAdapter candleLight;
        VirtualBusPort virtualPort;
        SlcanAdapter virtualBus{ virtualPort };
        MultiCanAdapter::WithMaxBackends<5> adapter;
#endif
    };
}

#endif
