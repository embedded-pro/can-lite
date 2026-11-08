#include "can-lite/tracing/TracingBypassCan.hpp"

namespace services
{
    TracingBypassCan::TracingBypassCan(hal::Can& traced, hal::Can& untraced, const infra::Function<bool(Id id)>& bypassesTracing)
        : traced(traced)
        , untraced(untraced)
        , bypassesTracing(bypassesTracing)
    {}

    void TracingBypassCan::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (bypassesTracing(id))
            untraced.SendData(id, data, actionOnCompletion);
        else
            traced.SendData(id, data, actionOnCompletion);
    }

    void TracingBypassCan::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        traced.ReceiveData(receivedAction);
    }
}
