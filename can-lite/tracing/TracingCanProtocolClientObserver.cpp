#include "can-lite/tracing/TracingCanProtocolClientObserver.hpp"
#include "infra/stream/OutputStream.hpp"

namespace services
{
    namespace
    {
        constexpr const char* tracePrefix = "TracingCanProtocolClientObserver: ";
    }

    TracingCanProtocolClientObserver::TracingCanProtocolClientObserver(CanProtocolClient& subject, Tracer& tracer)
        : CanProtocolClientObserver(subject)
        , tracer(tracer)
    {}

    void TracingCanProtocolClientObserver::OnServerOnline(uint16_t nodeId)
    {
        tracer.Trace() << tracePrefix << "ServerOnline" << infra::hex << " node 0x" << nodeId;
    }

    void TracingCanProtocolClientObserver::OnServerOffline(uint16_t nodeId)
    {
        tracer.Trace() << tracePrefix << "ServerOffline" << infra::hex << " node 0x" << nodeId;
    }

    void TracingCanProtocolClientObserver::OnCommandAckTimeout(uint16_t nodeId, uint8_t category, uint8_t messageType)
    {
        tracer.Trace() << tracePrefix << "CommandAckTimeout" << infra::hex << " node 0x" << nodeId
                       << " cat 0x" << category << " type 0x" << messageType;
    }
}
