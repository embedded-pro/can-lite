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
        if (payload.size() < 3)
            return false;

        if (onCommandAck)
            onCommandAck(payload[0], payload[1], static_cast<CanAckStatus>(payload[2]));

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
