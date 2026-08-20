#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>

namespace services
{
    class TracingCan
        : public hal::Can
    {
    public:
        TracingCan(hal::Can& can, Tracer& tracer);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

    private:
        void TraceFrame(const char* direction, Id id, const Message& data);
        void TraceExtendedFrame(const char* direction, uint32_t rawId, const Message& data);
        void TraceCommandAck(const Message& data);

        hal::Can& can;
        Tracer& tracer;

        infra::Function<void(Id id, const Message& data)> receivedAction;
        infra::AutoResetFunction<void(bool success)> onSendDone;
    };
}
