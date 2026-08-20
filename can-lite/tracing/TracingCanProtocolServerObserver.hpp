#pragma once

#include "can-lite/server/CanProtocolServer.hpp"
#include "services/tracer/Tracer.hpp"

namespace services
{
    class TracingCanProtocolServerObserver
        : public CanProtocolServerObserver
    {
    public:
        TracingCanProtocolServerObserver(CanProtocolServer& subject, Tracer& tracer);

        void Online() override;
        void Offline() override;

    private:
        Tracer& tracer;
    };
}
