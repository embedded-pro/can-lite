#include "can-lite/categories/system/CanSystemCategoryClient.hpp"

namespace services
{
    CanSystemCategoryClient::CanSystemCategoryClient()
        : CanCategoryClient(messageTypeStorage)
    {
        AddMessageType(canCommandAckMessageTypeId, [this](infra::ConstByteRange payload)
            {
                return HandleCommandAck(payload);
            });
        AddMessageType(canCategoryListResponseMessageTypeId, [this](infra::ConstByteRange payload)
            {
                return HandleCategoryListResponse(payload);
            });
    }

    uint8_t CanSystemCategoryClient::Id() const
    {
        return canSystemCategoryId;
    }

    bool CanSystemCategoryClient::RequiresSequenceValidation() const
    {
        return false;
    }

    bool CanSystemCategoryClient::HandleCommandAck(infra::ConstByteRange payload)
    {
        if (payload.size() < canCommandAckSize)
            return false;

        CanCommandAck ack{ payload[0], payload[1], static_cast<CanAckStatus>(payload[2]), payload[3], payload[4] };

        NotifyObservers([&ack](auto& observer)
            {
                observer.OnCommandAck(ack);
            });

        return true;
    }

    bool CanSystemCategoryClient::HandleCategoryListResponse(infra::ConstByteRange payload)
    {
        NotifyObservers([payload](auto& observer)
            {
                observer.OnCategoryListResponse(payload);
            });

        return true;
    }
}
