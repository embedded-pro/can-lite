#pragma once

#include "can-lite/transport/IsoTpTransport.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>

namespace services
{
    class TracingIsoTpTransport
        : public IsoTpTransport
    {
    public:
        TracingIsoTpTransport(IsoTpTransport& transport, Tracer& tracer);

        bool RegisterReceiveChannel(uint32_t dataId, uint32_t fcId) override;
        void ReleaseChannel(uint32_t dataId) override;
        bool SendPdu(uint32_t dataId, uint32_t fcId, infra::ConstByteRange pdu,
            const infra::Function<void()>& onDone) override;
        bool ProcessFrame(uint32_t canId, const hal::Can::Message& frame) override;
        void SetOnPduReceived(
            infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> callback) override;
        void SetOnAbort(
            infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> callback) override;

    private:
        IsoTpTransport& transport;
        Tracer& tracer;

        infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> onPduReceived;
        infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> onAbort;
    };
}
