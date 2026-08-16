#include "can-lite/core/CanCategory.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    CanCategory::CanCategory(infra::BoundedVector<CanMessageTypeBinding>& messageTypes)
        : messageTypes(messageTypes)
    {}

    void CanCategory::AddMessageType(uint8_t messageType, const CanMessageHandler& handler)
    {
        for (const auto& binding : messageTypes)
            really_assert(binding.messageType != messageType);

        really_assert(!messageTypes.full());
        messageTypes.push_back(CanMessageTypeBinding{ messageType, handler });
    }

    CanDispatchResult CanCategory::HandleMessage(uint8_t messageType, infra::ConstByteRange payload) const
    {
        for (const auto& binding : messageTypes)
            if (binding.messageType == messageType)
                return binding.handler(payload) ? CanDispatchResult::handled : CanDispatchResult::rejected;

        return CanDispatchResult::unknownMessageType;
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
}
