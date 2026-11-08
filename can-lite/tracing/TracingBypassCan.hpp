#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"

namespace services
{
    class TracingBypassCan
        : public hal::Can
    {
    public:
        TracingBypassCan(hal::Can& traced, hal::Can& untraced, const infra::Function<bool(Id id)>& bypassesTracing);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

    private:
        hal::Can& traced;
        hal::Can& untraced;
        infra::Function<bool(Id id)> bypassesTracing;
    };
}
