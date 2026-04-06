#include "can-lite/core/CanFrameTransport.hpp"

namespace services
{
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
        onSendNotification = callback;
    }

    bool CanFrameTransport::SendFrame(CanPriority priority, uint8_t category, uint8_t messageType,
        const hal::Can::Message& data, const infra::Function<void()>& onDone)
    {
        return SendFrame(nodeId, priority, category, messageType, data, onDone);
    }

    bool CanFrameTransport::SendFrame(uint16_t targetNodeId, CanPriority priority, uint8_t category, uint8_t messageType,
        const hal::Can::Message& data, const infra::Function<void()>& onDone)
    {
        auto rawId = MakeCanId(priority, category, messageType, targetNodeId);
        return SendRawFrame(hal::Can::Id::Create29BitId(rawId), data, onDone);
    }

    bool CanFrameTransport::SendRawFrame(hal::Can::Id id, const hal::Can::Message& data,
        const infra::Function<void()>& onDone)
    {
        if (!sendInProgress)
        {
            sendInProgress = true;
            currentOnDone = onDone;
            can.SendData(id, data, [this](bool)
                {
                    auto done = currentOnDone;
                    SendNextQueued();
                    done();
                });
            if (onSendNotification)
                onSendNotification();
            return true;
        }

        if (sendQueue.full())
            return false;

        sendQueue.push_back(PendingFrame{ id, data, onDone });
        if (onSendNotification)
            onSendNotification();
        return true;
    }

    void CanFrameTransport::SendNextQueued()
    {
        if (sendQueue.empty())
        {
            sendInProgress = false;
            return;
        }

        auto frame = sendQueue.front();
        sendQueue.pop_front();
        currentOnDone = frame.onDone;

        can.SendData(frame.id, frame.data, [this](bool)
            {
                auto done = currentOnDone;
                SendNextQueued();
                done();
            });
    }
}
