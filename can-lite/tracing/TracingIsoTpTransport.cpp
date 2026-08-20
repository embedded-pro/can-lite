#include "can-lite/tracing/TracingIsoTpTransport.hpp"
#include "infra/stream/OutputStream.hpp"

namespace services
{
    namespace
    {
        constexpr const char* tracePrefix = "TracingIsoTpTransport: ";

        const char* OutcomeName(bool accepted)
        {
            return accepted ? "accepted" : "rejected";
        }

        const char* AbortReasonName(iso_tp::AbortReason reason)
        {
            switch (reason)
            {
                case iso_tp::AbortReason::nBsTimeout:
                    return "nBsTimeout";
                case iso_tp::AbortReason::nCrTimeout:
                    return "nCrTimeout";
                case iso_tp::AbortReason::overflow:
                    return "overflow";
                case iso_tp::AbortReason::unexpectedFrame:
                    return "unexpectedFrame";
                case iso_tp::AbortReason::waitLimitExceeded:
                    return "waitLimitExceeded";
            }
            return "unknown";
        }
    }

    TracingIsoTpTransport::TracingIsoTpTransport(IsoTpTransport& transport, Tracer& tracer)
        : transport(transport)
        , tracer(tracer)
    {}

    bool TracingIsoTpTransport::RegisterReceiveChannel(uint32_t dataId, uint32_t fcId)
    {
        auto accepted = transport.RegisterReceiveChannel(dataId, fcId);

        tracer.Trace() << tracePrefix << "RegisterReceiveChannel" << infra::hex
                       << " dataId 0x" << dataId << " fcId 0x" << fcId << " " << OutcomeName(accepted);

        return accepted;
    }

    void TracingIsoTpTransport::ReleaseChannel(uint32_t dataId)
    {
        tracer.Trace() << tracePrefix << "ReleaseChannel" << infra::hex << " dataId 0x" << dataId;

        transport.ReleaseChannel(dataId);
    }

    bool TracingIsoTpTransport::SendPdu(uint32_t dataId, uint32_t fcId, infra::ConstByteRange pdu,
        const infra::Function<void()>& onDone)
    {
        auto accepted = transport.SendPdu(dataId, fcId, pdu, onDone);

        tracer.Trace() << tracePrefix << "SendPdu" << infra::hex
                       << " dataId 0x" << dataId << " fcId 0x" << fcId
                       << " size 0x" << pdu.size() << " " << OutcomeName(accepted);

        return accepted;
    }

    bool TracingIsoTpTransport::ProcessFrame(uint32_t canId, const hal::Can::Message& frame)
    {
        auto claimed = transport.ProcessFrame(canId, frame);

        if (claimed)
            tracer.Trace() << tracePrefix << "ProcessFrame" << infra::hex << " canId 0x" << canId
                           << " dlc " << frame.size() << " data " << infra::AsHex(infra::MakeRange(frame));

        return claimed;
    }

    void TracingIsoTpTransport::SetOnPduReceived(
        infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> callback)
    {
        onPduReceived = callback;

        transport.SetOnPduReceived([this](uint32_t dataId, infra::ConstByteRange pdu)
            {
                tracer.Trace() << tracePrefix << "PduReceived" << infra::hex << " dataId 0x" << dataId
                               << " size 0x" << pdu.size() << " data " << infra::AsHex(pdu);
                onPduReceived(dataId, pdu);
            });
    }

    void TracingIsoTpTransport::SetOnAbort(
        infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> callback)
    {
        onAbort = callback;

        transport.SetOnAbort([this](uint32_t dataId, iso_tp::AbortReason reason)
            {
                tracer.Trace() << tracePrefix << "Abort" << infra::hex << " dataId 0x" << dataId
                               << " reason " << AbortReasonName(reason);
                onAbort(dataId, reason);
            });
    }
}
