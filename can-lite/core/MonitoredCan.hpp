#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/util/Function.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace services
{
    enum class CanBusError : uint8_t
    {
        busOff,
        errorPassive,
        errorWarning,
        messageLost,
        rxBufferOverflow,
        ackError,
        stuffError,
        formError,
        crcError,
        bit0Error,
        bit1Error,
        other,
    };

    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, CanBusError error);

    class CanBusErrorCounters
    {
    public:
        static constexpr std::size_t errorClasses{ static_cast<std::size_t>(CanBusError::other) + 1 };

        void Record(CanBusError error);
        void Reset();
        uint32_t Count(CanBusError error) const;
        uint32_t Total() const;

    private:
        static std::size_t IndexOf(CanBusError error);
        static uint32_t Increment(uint32_t counter);

        std::array<uint32_t, errorClasses> counters{};
        uint32_t total{ 0 };
    };

    class MonitoredCan
        : public hal::Can
    {
    public:
        virtual void SetOnError(const infra::Function<void(CanBusError)>& handler) = 0;

        const CanBusErrorCounters& ErrorStatistics() const;
        void ResetErrorStatistics();

    protected:
        ~MonitoredCan() = default;

        void RecordError(CanBusError error);

    private:
        CanBusErrorCounters errorCounters;
    };

    template<std::derived_from<hal::Can> Driver>
    class MonitoredCanImpl
        : public MonitoredCan
    {
    public:
        MonitoredCanImpl() = default;

        template<typename... Args>
        requires(sizeof...(Args) > 0 && (!std::same_as<std::remove_cvref_t<Args>, MonitoredCanImpl> && ...))
        explicit MonitoredCanImpl(Args&&... args)
            : can(std::forward<Args>(args)...)
        {}

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;
        void SetOnError(const infra::Function<void(CanBusError)>& handler) override;
        void InvokeErrorHandler(CanBusError error);

    private:
        Driver can;
        infra::Function<void(CanBusError)> onError;
    };

    template<std::derived_from<hal::Can> Driver>
    void MonitoredCanImpl<Driver>::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        can.SendData(id, data, actionOnCompletion);
    }

    template<std::derived_from<hal::Can> Driver>
    void MonitoredCanImpl<Driver>::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        can.ReceiveData(receivedAction);
    }

    template<std::derived_from<hal::Can> Driver>
    void MonitoredCanImpl<Driver>::SetOnError(const infra::Function<void(CanBusError)>& handler)
    {
        onError = handler;
    }

    template<std::derived_from<hal::Can> Driver>
    void MonitoredCanImpl<Driver>::InvokeErrorHandler(CanBusError error)
    {
        RecordError(error);

        if (onError)
            onError(error);
    }
}
