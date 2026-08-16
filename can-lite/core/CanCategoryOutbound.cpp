#include "can-lite/core/CanCategoryOutbound.hpp"

namespace services
{
    CanCategoryOutboundNull& CanCategoryOutboundNull::Instance()
    {
        static CanCategoryOutboundNull instance;
        return instance;
    }

    uint8_t CanCategoryOutboundNull::Category() const
    {
        return 0;
    }

    uint16_t CanCategoryOutboundNull::NodeId() const
    {
        return 0;
    }

    uint16_t CanCategoryOutboundNull::PeerNodeId() const
    {
        return 0;
    }

    uint8_t CanCategoryOutboundNull::Correlation() const
    {
        return 0;
    }

    bool CanCategoryOutboundNull::Send(CanPriority, uint8_t, const hal::Can::Message&)
    {
        return false;
    }

    bool CanCategoryOutboundNull::SendTo(uint16_t, CanPriority, uint8_t, const hal::Can::Message&)
    {
        return false;
    }

    bool CanCategoryOutboundNull::SendSequencedTo(uint16_t, CanPriority, uint8_t, const hal::Can::Message&)
    {
        return false;
    }

    void CanCategoryOutboundNull::SendAck(uint8_t, CanAckStatus)
    {}

    void CanCategoryOutboundImpl::Bind(CanFrameTransport& newTransport, uint8_t newCategory)
    {
        transport = &newTransport;
        category = newCategory;
        peerNodeId = 0;
        correlation = 0;
        sequences.Forget();
    }

    void CanCategoryOutboundImpl::Unbind()
    {
        transport = nullptr;
        category = 0;
        peerNodeId = 0;
        correlation = 0;
        sequences.Forget();
    }

    bool CanCategoryOutboundImpl::IsBound() const
    {
        return transport != nullptr;
    }

    bool CanCategoryOutboundImpl::IsBoundTo(uint8_t queriedCategory) const
    {
        return transport != nullptr && category == queriedCategory;
    }

    void CanCategoryOutboundImpl::BeginRequest(uint16_t requestPeerNodeId, uint8_t requestCorrelation)
    {
        peerNodeId = requestPeerNodeId;
        correlation = requestCorrelation;
    }

    CanSequenceTable::ValidationResult CanCategoryOutboundImpl::ValidateSequence(uint16_t peer, uint8_t sequenceNumber)
    {
        return sequences.Validate(peer, sequenceNumber);
    }

    void CanCategoryOutboundImpl::ResyncSequence(uint16_t peer, uint8_t nextSequenceNumber)
    {
        sequences.Resync(peer, nextSequenceNumber);
    }

    void CanCategoryOutboundImpl::SendAckWith(uint16_t targetNodeId, uint8_t messageType, CanAckStatus status,
        uint8_t ackCorrelation, uint8_t expectedSequence)
    {
        if (transport == nullptr)
            return;

        hal::Can::Message msg;
        msg.push_back(category);
        msg.push_back(messageType);
        msg.push_back(static_cast<uint8_t>(status));
        msg.push_back(ackCorrelation);
        msg.push_back(expectedSequence);

        transport->SendFrame(targetNodeId, CanPriority::response, canSystemCategoryId,
            canCommandAckMessageTypeId, msg, [] {});
    }

    uint8_t CanCategoryOutboundImpl::Category() const
    {
        return category;
    }

    uint16_t CanCategoryOutboundImpl::NodeId() const
    {
        return transport != nullptr ? transport->NodeId() : uint16_t{ 0 };
    }

    uint16_t CanCategoryOutboundImpl::PeerNodeId() const
    {
        return peerNodeId;
    }

    uint8_t CanCategoryOutboundImpl::Correlation() const
    {
        return correlation;
    }

    bool CanCategoryOutboundImpl::Send(CanPriority priority, uint8_t messageType, const hal::Can::Message& payload)
    {
        if (transport == nullptr)
            return false;

        return transport->SendFrame(priority, category, messageType, payload, [] {});
    }

    bool CanCategoryOutboundImpl::SendTo(uint16_t targetNodeId, CanPriority priority, uint8_t messageType,
        const hal::Can::Message& payload)
    {
        if (transport == nullptr)
            return false;

        return transport->SendFrame(targetNodeId, priority, category, messageType, payload, [] {});
    }

    bool CanCategoryOutboundImpl::SendSequencedTo(uint16_t targetNodeId, CanPriority priority, uint8_t messageType,
        const hal::Can::Message& payload)
    {
        if (transport == nullptr)
            return false;

        if (payload.size() >= payload.max_size())
            return false;

        hal::Can::Message sequenced;
        sequenced.push_back(sequences.Allocate(targetNodeId));
        for (auto byte : payload)
            sequenced.push_back(byte);

        return transport->SendFrame(targetNodeId, priority, category, messageType, sequenced, [] {});
    }

    void CanCategoryOutboundImpl::SendAck(uint8_t messageType, CanAckStatus status)
    {
        SendAckWith(NodeId(), messageType, status, correlation, 0);
    }
}
