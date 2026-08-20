#pragma once

#include "can-lite/client/CanProtocolClient.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>

namespace services
{
    class TracingCanProtocolClientObserver
        : public CanProtocolClientObserver
    {
    public:
        TracingCanProtocolClientObserver(CanProtocolClient& subject, Tracer& tracer);

        void OnServerOnline(uint16_t nodeId) override;
        void OnServerOffline(uint16_t nodeId) override;
        void OnCommandAckTimeout(uint16_t nodeId, uint8_t category, uint8_t messageType) override;

    private:
        Tracer& tracer;
    };
}
