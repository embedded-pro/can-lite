#include "can-lite/core/MonitoredCan.hpp"

namespace services
{
    namespace
    {
        const char* CanBusErrorName(CanBusError error)
        {
            using enum CanBusError;
            switch (error)
            {
                case busOff:
                    return "bus off";
                case errorPassive:
                    return "error passive";
                case errorWarning:
                    return "error warning";
                case messageLost:
                    return "message lost";
                case rxBufferOverflow:
                    return "rx buffer overflow";
                case ackError:
                    return "ack error";
                case stuffError:
                    return "stuff error";
                case formError:
                    return "form error";
                case crcError:
                    return "crc error";
                case bit0Error:
                    return "bit0 error";
                case bit1Error:
                    return "bit1 error";
                case other:
                    return "other";
            }

            return "unknown";
        }
    }

    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, CanBusError error)
    {
        return stream << CanBusErrorName(error);
    }

    void CanBusErrorCounters::Record(CanBusError error)
    {
        auto& counter = counters[IndexOf(error)];
        counter = SaturatingIncrement(counter);
        total = SaturatingIncrement(total);
    }

    void CanBusErrorCounters::Reset()
    {
        counters.fill(0);
        total = 0;
    }

    uint32_t CanBusErrorCounters::Count(CanBusError error) const
    {
        return counters[IndexOf(error)];
    }

    uint32_t CanBusErrorCounters::Total() const
    {
        return total;
    }

    uint32_t CanBusErrorCounters::SaturatingIncrement(uint32_t counter)
    {
        return counter + static_cast<uint32_t>(counter != UINT32_MAX);
    }

    std::size_t CanBusErrorCounters::IndexOf(CanBusError error)
    {
        const auto index = static_cast<std::size_t>(error);

        return index < errorClasses ? index : errorClasses - 1;
    }

    const CanBusErrorCounters& MonitoredCan::ErrorStatistics() const
    {
        return errorCounters;
    }

    void MonitoredCan::ResetErrorStatistics()
    {
        errorCounters.Reset();
    }

    void MonitoredCan::RecordError(CanBusError error)
    {
        errorCounters.Record(error);
    }
}
