#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    void CanCategory::AddMessageType(CanMessageType& messageType)
    {
        messageTypes.push_back(messageType);
    }

    bool CanCategory::HandleMessage(uint8_t messageType, const hal::Can::Message& data)
    {
        for (auto& handler : messageTypes)
        {
            if (handler.Id() == messageType)
            {
                handler.Handle(data);
                return true;
            }
        }

        return false;
    }

    bool CanCategory::HandlePduMessage(uint8_t messageType, infra::ConstByteRange pdu)
    {
        for (auto& handler : messageTypes)
        {
            if (handler.Id() == messageType)
                return handler.HandlePdu(pdu);
        }

        return false;
    }

    CanCategoryServer::CanCategoryServer(CanFrameTransport& transport)
        : transport(transport)
    {}

    bool CanCategoryServer::RequiresSequenceValidation() const
    {
        return true;
    }

    void CanCategoryServer::SetAcknowledger(CanCommandAcknowledger& ack)
    {
        acknowledger = &ack;
    }

    void CanCategoryServer::SendCommandAck(uint8_t messageType, CanAckStatus status)
    {
        really_assert(acknowledger != nullptr);
        acknowledger->SendCommandAck(Id(), messageType, status);
    }

    CanFrameTransport& CanCategoryServer::Transport()
    {
        return transport;
    }

    bool CanCategoryServer::SendResponse(uint8_t messageType, const hal::Can::Message& data)
    {
        return transport.SendFrame(CanPriority::response, Id(), messageType, data, [](bool) {});
    }

    bool CanCategoryServer::SendResponse(uint8_t messageType, const CanPayloadWriter& payload)
    {
        return payload.Valid() && SendResponse(messageType, payload.Message());
    }

    bool CanCategoryServer::SendTelemetry(uint8_t messageType, const hal::Can::Message& data)
    {
        return transport.SendFrame(CanPriority::telemetry, Id(), messageType, data, [](bool) {});
    }

    bool CanCategoryServer::SendTelemetry(uint8_t messageType, const CanPayloadWriter& payload)
    {
        return payload.Valid() && SendTelemetry(messageType, payload.Message());
    }

    bool CanCategoryServer::SendCategoryError(uint8_t originatingCommandId, uint8_t categoryErrorCode)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(originatingCommandId).WriteUInt8(categoryErrorCode);

        return SendResponse(canCategoryErrorResponseMessageTypeId, payload);
    }

    CanCategoryClient::CanCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource)
        : transport(transport)
        , sequenceSource(sequenceSource)
    {}

    bool CanCategoryClient::RequiresSequenceValidation() const
    {
        return false;
    }

    CanFrameTransport& CanCategoryClient::Transport()
    {
        return transport;
    }

    bool CanCategoryClient::SendCommand(uint16_t targetNodeId, uint8_t messageType, CanPriority priority)
    {
        return SendCommand(targetNodeId, messageType, hal::Can::Message{}, priority);
    }

    bool CanCategoryClient::SendCommand(uint16_t targetNodeId, uint8_t messageType, const hal::Can::Message& payload, CanPriority priority)
    {
        CanPayloadWriter data;
        data.WriteUInt8(sequenceSource.PeekSequence(targetNodeId)).WriteBytes(infra::MakeRange(payload));

        if (!SendCommandWithoutSequence(targetNodeId, messageType, data, priority))
            return false;

        sequenceSource.CommitSequence(targetNodeId, Id(), messageType);
        return true;
    }

    bool CanCategoryClient::SendCommand(uint16_t targetNodeId, uint8_t messageType, const CanPayloadWriter& payload, CanPriority priority)
    {
        return payload.Valid() && SendCommand(targetNodeId, messageType, payload.Message(), priority);
    }

    bool CanCategoryClient::SendCommandWithoutSequence(uint16_t targetNodeId, uint8_t messageType, CanPriority priority)
    {
        return SendCommandWithoutSequence(targetNodeId, messageType, hal::Can::Message{}, priority);
    }

    bool CanCategoryClient::SendCommandWithoutSequence(uint16_t targetNodeId, uint8_t messageType, const hal::Can::Message& payload, CanPriority priority)
    {
        return transport.SendFrame(targetNodeId, priority, Id(), messageType, payload, [](bool) {});
    }

    bool CanCategoryClient::SendCommandWithoutSequence(uint16_t targetNodeId, uint8_t messageType, const CanPayloadWriter& payload, CanPriority priority)
    {
        return payload.Valid() && SendCommandWithoutSequence(targetNodeId, messageType, payload.Message(), priority);
    }
}
