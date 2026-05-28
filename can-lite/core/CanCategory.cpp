#include "can-lite/core/CanCategory.hpp"
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

    bool CanCategoryClient::RequiresSequenceValidation() const
    {
        return false;
    }
}
