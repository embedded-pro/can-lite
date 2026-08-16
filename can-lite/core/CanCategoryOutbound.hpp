#pragma once

#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceTable.hpp"
#include "hal/interfaces/Can.hpp"
#include <cstdint>

namespace services
{
    // The outbound half of a registered category. The protocol host creates one
    // handle per category at registration and binds it to that category's ID, so
    // a category never composes a CAN identifier itself. The handle owns the
    // category's sequence allocation and carries the acknowledger, which is why
    // sending an acknowledgement needs no null check.
    class CanCategoryOutbound
    {
    public:
        virtual uint8_t Category() const = 0;
        virtual uint16_t NodeId() const = 0;

        // The peer and sequence number of the request currently being served,
        // so an acknowledgement can correlate itself to the request.
        virtual uint16_t PeerNodeId() const = 0;
        virtual uint8_t Correlation() const = 0;

        // Sends from this node's own address, which is how a server addresses
        // its responses.
        virtual bool Send(CanPriority priority, uint8_t messageType, const hal::Can::Message& payload) = 0;
        // Sends to a specific node, which is how a client addresses its commands.
        virtual bool SendTo(uint16_t targetNodeId, CanPriority priority, uint8_t messageType,
            const hal::Can::Message& payload) = 0;
        // As SendTo, but prepends the next sequence number allocated for this
        // (peer, category) pair. Fails when the payload leaves no room for it.
        virtual bool SendSequencedTo(uint16_t targetNodeId, CanPriority priority, uint8_t messageType,
            const hal::Can::Message& payload) = 0;

        virtual void SendAck(uint8_t messageType, CanAckStatus status) = 0;

    protected:
        CanCategoryOutbound() = default;
        CanCategoryOutbound(const CanCategoryOutbound&) = delete;
        CanCategoryOutbound& operator=(const CanCategoryOutbound&) = delete;
        ~CanCategoryOutbound() = default;
    };

    // The handle a category holds while it is not registered anywhere. Sending
    // through it is a silent no-op, so an unregistered category can never abort
    // the node.
    class CanCategoryOutboundNull
        : public CanCategoryOutbound
    {
    public:
        static CanCategoryOutboundNull& Instance();

        uint8_t Category() const override;
        uint16_t NodeId() const override;
        uint16_t PeerNodeId() const override;
        uint8_t Correlation() const override;

        bool Send(CanPriority priority, uint8_t messageType, const hal::Can::Message& payload) override;
        bool SendTo(uint16_t targetNodeId, CanPriority priority, uint8_t messageType,
            const hal::Can::Message& payload) override;
        bool SendSequencedTo(uint16_t targetNodeId, CanPriority priority, uint8_t messageType,
            const hal::Can::Message& payload) override;

        void SendAck(uint8_t messageType, CanAckStatus status) override;
    };

    // The handle the protocol host hands out. Bound to a transport and a
    // category ID for as long as the category is registered.
    class CanCategoryOutboundImpl
        : public CanCategoryOutbound
    {
    public:
        void Bind(CanFrameTransport& transport, uint8_t category);
        void Unbind();
        bool IsBound() const;
        bool IsBoundTo(uint8_t category) const;

        // Records which request the category is serving, so that any
        // acknowledgement it produces correlates to that request.
        void BeginRequest(uint16_t peerNodeId, uint8_t correlation);

        CanSequenceTable::ValidationResult ValidateSequence(uint16_t peerNodeId, uint8_t sequenceNumber);
        void ResyncSequence(uint16_t peerNodeId, uint8_t nextSequenceNumber);

        // The full acknowledgement, used by the host for statuses a category
        // cannot raise itself.
        void SendAckWith(uint16_t targetNodeId, uint8_t messageType, CanAckStatus status,
            uint8_t correlation, uint8_t expectedSequence);

        uint8_t Category() const override;
        uint16_t NodeId() const override;
        uint16_t PeerNodeId() const override;
        uint8_t Correlation() const override;

        bool Send(CanPriority priority, uint8_t messageType, const hal::Can::Message& payload) override;
        bool SendTo(uint16_t targetNodeId, CanPriority priority, uint8_t messageType,
            const hal::Can::Message& payload) override;
        bool SendSequencedTo(uint16_t targetNodeId, CanPriority priority, uint8_t messageType,
            const hal::Can::Message& payload) override;

        void SendAck(uint8_t messageType, CanAckStatus status) override;

    private:
        CanFrameTransport* transport = nullptr;
        uint8_t category = 0;
        uint16_t peerNodeId = 0;
        uint8_t correlation = 0;
        CanSequenceTable sequences;
    };
}
