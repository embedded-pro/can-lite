#include "can-lite/tracing/TracingCanProtocolServerObserver.hpp"
#include "infra/stream/OutputStream.hpp"

namespace services
{
    namespace
    {
        constexpr const char* tracePrefix = "TracingCanProtocolServerObserver: ";
    }

    TracingCanProtocolServerObserver::TracingCanProtocolServerObserver(CanProtocolServer& subject, Tracer& tracer)
        : CanProtocolServerObserver(subject)
        , tracer(tracer)
    {}

    void TracingCanProtocolServerObserver::Online()
    {
        tracer.Trace() << tracePrefix << "Online";
    }

    void TracingCanProtocolServerObserver::Offline()
    {
        tracer.Trace() << tracePrefix << "Offline";
    }
}
