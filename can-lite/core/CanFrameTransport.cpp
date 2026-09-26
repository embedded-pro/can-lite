#include "can-lite/core/CanFrameTransport.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services
{
    namespace
    {
        uint8_t PriorityOf(hal::Can::Id id)
        {
            if (!id.Is29BitId())
                return static_cast<uint8_t>(CanPriority::command);

            return static_cast<uint8_t>(ExtractCanPriority(id.Get29BitId()));
        }
    }

    CanFrameTransport::PendingFrame::PendingFrame(hal::Can::Id id, const hal::Can::Message& data, const infra::Function<void(bool success)>& onDone)
        : id(id)
        , data(data)
        , onDone(onDone)
        , priority(PriorityOf(id))
    {}

    bool CanFrameTransport::PendingFrame::IsEmergency() const
    {
        return priority == static_cast<uint8_t>(CanPriority::emergency);
    }

    CanFrameTransport::CanFrameTransport(hal::Can& can, uint16_t nodeId)
        : can(can)
        , nodeId(nodeId)
    {}

    void CanFrameTransport::SetNodeId(uint16_t newNodeId)
    {
        nodeId = newNodeId;
    }

    uint16_t CanFrameTransport::NodeId() const
    {
        return nodeId;
    }

    void CanFrameTransport::SetOnSendNotification(infra::Function<void()> callback)
    {
        really_assert(!onSendNotification);
        onSendNotification = callback;
    }

    void CanFrameTransport::ClearOnSendNotification()
    {
        onSendNotification = nullptr;
    }

    bool CanFrameTransport::SendFrame(CanPriority priority, uint8_t category, uint8_t messageType,
        const hal::Can::Message& data, const infra::Function<void(bool success)>& onDone)
    {
        return SendFrame(nodeId, priority, category, messageType, data, onDone);
    }

    bool CanFrameTransport::SendFrame(uint16_t targetNodeId, CanPriority priority, uint8_t category, uint8_t messageType,
        const hal::Can::Message& data, const infra::Function<void(bool success)>& onDone)
    {
        auto rawId = MakeCanId(priority, category, messageType, targetNodeId);
        return SendRawFrame(hal::Can::Id::Create29BitId(rawId), data, onDone);
    }

    bool CanFrameTransport::SendRawFrame(hal::Can::Id id, const hal::Can::Message& data,
        const infra::Function<void(bool success)>& onDone)
    {
        PendingFrame frame{ id, data, onDone };

        if (current)
            return Enqueue(frame);

        current.emplace(frame);
        currentRetries = 0;
        Transmit();
        NotifySend();
        return true;
    }

    const CanFrameTransport::Counters& CanFrameTransport::Statistics() const
    {
        return counters;
    }

    bool CanFrameTransport::Enqueue(const PendingFrame& frame)
    {
        if (frame.IsEmergency() ? !EnqueueEmergency(frame) : !EnqueueOrdinary(frame))
            return false;

        NotifySend();
        return true;
    }

    bool CanFrameTransport::EnqueueOrdinary(const PendingFrame& frame)
    {
        if (OrdinaryCount() >= queueDepth - emergencyReserve)
        {
            ++counters.ordinaryDrops;
            return false;
        }

        Insert(frame);
        return true;
    }

    bool CanFrameTransport::EnqueueEmergency(const PendingFrame& frame)
    {
        if (!sendQueue.full())
        {
            Insert(frame);
            return true;
        }

        if (sendQueue.back().IsEmergency())
        {
            ++counters.emergencyDrops;
            return false;
        }

        auto evicted = sendQueue.back().onDone;
        sendQueue.pop_back();
        ++counters.evictions;
        Insert(frame);
        evicted(false);
        return true;
    }

    void CanFrameTransport::Insert(const PendingFrame& frame)
    {
        auto isLessUrgent = [&frame](const PendingFrame& queued)
        {
            return queued.priority > frame.priority;
        };
        auto index = std::find_if(sendQueue.begin(), sendQueue.end(), isLessUrgent) - sendQueue.begin();

        sendQueue.push_back(frame);
        std::rotate(sendQueue.begin() + index, sendQueue.end() - 1, sendQueue.end());
    }

    std::size_t CanFrameTransport::OrdinaryCount() const
    {
        return static_cast<std::size_t>(std::count_if(sendQueue.begin(), sendQueue.end(), [](const PendingFrame& queued)
            {
                return !queued.IsEmergency();
            }));
    }

    void CanFrameTransport::Transmit()
    {
        auto id = current->id;
        auto data = current->data;

        can.SendData(id, data, [this](bool success)
            {
                OnSendComplete(success);
            });
    }

    void CanFrameTransport::OnSendComplete(bool success)
    {
        if (RetryCurrent(success))
            return;

        auto done = current->onDone;
        SendNextQueued();
        done(success);
    }

    bool CanFrameTransport::RetryCurrent(bool success)
    {
        if (success)
            return false;

        if (!current->IsEmergency())
        {
            ++counters.sendFailures;
            return false;
        }

        if (currentRetries == emergencyRetryLimit)
        {
            ++counters.emergencyDrops;
            return false;
        }

        ++currentRetries;
        ++counters.emergencyRetries;
        Transmit();
        return true;
    }

    void CanFrameTransport::SendNextQueued()
    {
        if (sendQueue.empty())
        {
            current.reset();
            return;
        }

        current.emplace(sendQueue.front());
        sendQueue.erase(sendQueue.begin());
        currentRetries = 0;
        Transmit();
    }

    void CanFrameTransport::NotifySend()
    {
        if (onSendNotification)
            onSendNotification();
    }
}
