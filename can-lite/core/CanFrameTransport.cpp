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

    void CanFrameTransport::SendFrame(CanPriority priority, uint8_t category, uint8_t messageType,
        const hal::Can::Message& data, const infra::Function<void()>& onDone)
    {
        SendFrame(nodeId, priority, category, messageType, data, onDone);
    }

    void CanFrameTransport::SendFrame(uint16_t targetNodeId, CanPriority priority, uint8_t category, uint8_t messageType,
        const hal::Can::Message& data, const infra::Function<void()>& onDone)
    {
        auto rawId = MakeCanId(priority, category, messageType, targetNodeId);
        auto canId = hal::Can::Id::Create29BitId(rawId);

        if (!sendInProgress)
        {
            sendInProgress = true;
            currentOnDone = onDone;
            can.SendData(canId, data, [this](bool)
                {
                    auto done = currentOnDone;
                    SendNextQueued();
                    done();
                });
        }
        else if (!sendQueue.full())
        {
            sendQueue.push_back(PendingFrame{ canId, data, onDone });
        }
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
