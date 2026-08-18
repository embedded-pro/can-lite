#pragma once

#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageType.hpp"
#include "can-lite/core/CanPayload.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceSource.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/IntrusiveList.hpp"
#include <cstdint>

namespace services
{
    class CanCommandAcknowledger
    {
    public:
        virtual void SendCommandAck(uint8_t category, uint8_t messageType, CanAckStatus status) = 0;

    protected:
        CanCommandAcknowledger() = default;
        CanCommandAcknowledger(const CanCommandAcknowledger&) = delete;
        CanCommandAcknowledger& operator=(const CanCommandAcknowledger&) = delete;
        ~CanCommandAcknowledger() = default;
    };

    class CanCategory
    {
    public:
        virtual uint8_t Id() const = 0;
        virtual bool RequiresSequenceValidation() const = 0;

        void AddMessageType(CanMessageType& messageType);

        template<class... MessageTypes>
        void AddMessageTypes(MessageTypes&... messageTypes)
        {
            (AddMessageType(messageTypes), ...);
        }

        bool HandleMessage(uint8_t messageType, const hal::Can::Message& data);
        bool HandlePduMessage(uint8_t messageType, infra::ConstByteRange pdu);

    protected:
        CanCategory() = default;
        CanCategory(const CanCategory&) = delete;
        CanCategory& operator=(const CanCategory&) = delete;
        ~CanCategory() = default;

    private:
        infra::IntrusiveList<CanMessageType> messageTypes;
    };

    class CanCategoryServer
        : public CanCategory
        , public infra::IntrusiveList<CanCategoryServer>::NodeType
    {
    public:
        bool RequiresSequenceValidation() const override;

        void SetAcknowledger(CanCommandAcknowledger& acknowledger);
        void SendCommandAck(uint8_t messageType, CanAckStatus status);

    protected:
        explicit CanCategoryServer(CanFrameTransport& transport);
        ~CanCategoryServer() = default;

        CanFrameTransport& Transport();

        bool SendResponse(uint8_t messageType, const hal::Can::Message& data);
        bool SendResponse(uint8_t messageType, const CanPayloadWriter& payload);
        bool SendTelemetry(uint8_t messageType, const hal::Can::Message& data);
        bool SendTelemetry(uint8_t messageType, const CanPayloadWriter& payload);
        bool SendCategoryError(uint8_t originatingCommandId, uint8_t categoryErrorCode);

    private:
        CanFrameTransport& transport;
        CanCommandAcknowledger* acknowledger{ nullptr };
    };

    class CanCategoryClient
        : public CanCategory
        , public infra::IntrusiveList<CanCategoryClient>::NodeType
    {
    public:
        bool RequiresSequenceValidation() const override;

    protected:
        CanCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource);
        ~CanCategoryClient() = default;

        CanFrameTransport& Transport();

        bool SendCommand(uint16_t targetNodeId, uint8_t messageType, CanPriority priority = CanPriority::command);
        bool SendCommand(uint16_t targetNodeId, uint8_t messageType, const hal::Can::Message& payload, CanPriority priority = CanPriority::command);
        bool SendCommand(uint16_t targetNodeId, uint8_t messageType, const CanPayloadWriter& payload, CanPriority priority = CanPriority::command);

        bool SendCommandWithoutSequence(uint16_t targetNodeId, uint8_t messageType, CanPriority priority = CanPriority::command);
        bool SendCommandWithoutSequence(uint16_t targetNodeId, uint8_t messageType, const hal::Can::Message& payload, CanPriority priority = CanPriority::command);
        bool SendCommandWithoutSequence(uint16_t targetNodeId, uint8_t messageType, const CanPayloadWriter& payload, CanPriority priority = CanPriority::command);

    private:
        CanFrameTransport& transport;
        CanSequenceSource& sequenceSource;
    };
}
