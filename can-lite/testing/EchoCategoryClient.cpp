#include "can-lite/testing/EchoCategoryClient.hpp"

namespace services
{
    EchoCategoryClient::EchoCategoryClient(uint8_t categoryId)
        : CanCategoryClient(messageTypeStorage)
        , categoryId(categoryId)
    {
        AddMessageType(echoReplyMessageTypeId, [this](infra::ConstByteRange payload)
            {
                return HandleEchoReply(payload);
            });
    }

    uint8_t EchoCategoryClient::Id() const
    {
        return categoryId;
    }

    bool EchoCategoryClient::RequiresSequenceValidation() const
    {
        return false;
    }

    bool EchoCategoryClient::SendEchoRequest(uint16_t targetNodeId, const hal::Can::Message& payload)
    {
        return Outbound().SendSequencedTo(targetNodeId, CanPriority::command, echoRequestMessageTypeId, payload);
    }

    bool EchoCategoryClient::SendValidatedRequest(uint16_t targetNodeId, const hal::Can::Message& payload)
    {
        return Outbound().SendSequencedTo(targetNodeId, CanPriority::command, echoValidatedRequestMessageTypeId, payload);
    }

    uint32_t EchoCategoryClient::ReplyCount() const
    {
        return replyCount;
    }

    bool EchoCategoryClient::HandleEchoReply(infra::ConstByteRange payload)
    {
        ++replyCount;

        NotifyObservers([payload](auto& observer)
            {
                observer.OnEchoReply(payload);
            });

        return true;
    }
}
