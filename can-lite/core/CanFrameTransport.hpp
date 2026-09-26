#pragma once

#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    class CanFrameTransport
    {
    public:
        static constexpr std::size_t queueDepth = 8;
        static constexpr std::size_t emergencyReserve = 2;
        static constexpr uint8_t emergencyRetryLimit = 2;

        struct Counters
        {
            uint32_t emergencyDrops{};
            uint32_t emergencyRetries{};
            uint32_t evictions{};
            uint32_t ordinaryDrops{};
            uint32_t sendFailures{};
        };

        CanFrameTransport(hal::Can& can, uint16_t nodeId);

        void SetNodeId(uint16_t nodeId);
        uint16_t NodeId() const;

        void SetOnSendNotification(infra::Function<void()> callback);
        void ClearOnSendNotification();

        bool SendFrame(CanPriority priority, uint8_t category, uint8_t messageType,
            const hal::Can::Message& data, const infra::Function<void(bool success)>& onDone);
        bool SendFrame(uint16_t targetNodeId, CanPriority priority, uint8_t category, uint8_t messageType,
            const hal::Can::Message& data, const infra::Function<void(bool success)>& onDone);
        bool SendRawFrame(hal::Can::Id id, const hal::Can::Message& data,
            const infra::Function<void(bool success)>& onDone);

        const Counters& Statistics() const;

    private:
        struct PendingFrame
        {
            hal::Can::Id id;
            hal::Can::Message data;
            infra::Function<void(bool success)> onDone;
            uint8_t priority;

            PendingFrame(hal::Can::Id id, const hal::Can::Message& data, const infra::Function<void(bool success)>& onDone);
            PendingFrame(PendingFrame&&) noexcept = default;
            PendingFrame& operator=(PendingFrame&&) noexcept = default;
            PendingFrame(const PendingFrame&) = default;
            PendingFrame& operator=(const PendingFrame&) = default;

            bool IsEmergency() const;
        };

        bool Enqueue(const PendingFrame& frame);
        bool EnqueueOrdinary(const PendingFrame& frame);
        bool EnqueueEmergency(const PendingFrame& frame);
        void Insert(const PendingFrame& frame);
        std::size_t OrdinaryCount() const;
        void Transmit();
        void OnSendComplete(bool success);
        bool RetryCurrent(bool success);
        void SendNextQueued();
        void NotifySend() const;

        hal::Can& can;
        uint16_t nodeId;
        std::optional<PendingFrame> current;
        uint8_t currentRetries{};
        Counters counters;
        infra::Function<void()> onSendNotification;
        infra::BoundedVector<PendingFrame>::WithMaxSize<queueDepth> sendQueue;
    };
}
