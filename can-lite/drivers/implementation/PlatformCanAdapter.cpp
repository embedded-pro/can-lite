#include "can-lite/drivers/implementation/PlatformCanAdapter.hpp"

#if defined(__linux__) || defined(_WIN32)

namespace services
{
    PlatformCanAdapter::PlatformCanAdapter()
    {
#if defined(__linux__)
        adapter.Add("", socketCan);
        adapter.Add("slcan", slcan, "CANable");
        adapter.Add("virtual", virtualBus);
#else
        adapter.Add("PCAN", pcan);
        adapter.Add("Kvaser", kvaser);
        adapter.Add("slcan", slcan, "CANable");
        adapter.Add("candleLight", candleLight);
        adapter.Add("virtual", virtualBus);
#endif
    }

    CanBusAdapter& PlatformCanAdapter::Adapter()
    {
        return adapter;
    }
}

#endif
