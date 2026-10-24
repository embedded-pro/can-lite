#include "can-lite/drivers/implementation/PlatformCanAdapter.hpp"

#if defined(__linux__) || defined(_WIN32)

namespace services
{
    PlatformCanAdapter::PlatformCanAdapter()
    {
#if defined(__linux__)
        adapter.Add("", socketCan);
        adapter.Add("slcan", slcan, "CANable");
#else
        adapter.Add("PCAN", pcan);
        adapter.Add("Kvaser", kvaser);
        adapter.Add("slcan", slcan, "CANable");
        adapter.Add("candleLight", candleLight);
#endif
    }

    CanBusAdapter& PlatformCanAdapter::Adapter()
    {
        return adapter;
    }
}

#endif
