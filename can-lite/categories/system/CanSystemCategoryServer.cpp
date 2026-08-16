#include "can-lite/categories/system/CanSystemCategoryServer.hpp"

namespace services
{
    CanSystemCategoryServer::CanSystemCategoryServer()
        : CanCategoryServer(messageTypeStorage)
    {
        AddMessageType(canHeartbeatMessageTypeId, [this](infra::ConstByteRange payload)
            {
                return HandleHeartbeat(payload);
            });
        AddMessageType(canStatusRequestMessageTypeId, [this](infra::ConstByteRange payload)
            {
                return HandleStatusRequest(payload);
            });
        AddMessageType(canCategoryListRequestMessageTypeId, [this](infra::ConstByteRange payload)
            {
                return HandleCategoryListRequest(payload);
            });
    }

    uint8_t CanSystemCategoryServer::Id() const
    {
        return canSystemCategoryId;
    }

    bool CanSystemCategoryServer::RequiresSequenceValidation() const
    {
        return false;
    }

    bool CanSystemCategoryServer::HandleHeartbeat(infra::ConstByteRange payload)
    {
        auto version = payload.empty() ? uint8_t{ 0 } : payload.front();

        NotifyObservers([version](auto& observer)
            {
                observer.OnHeartbeatReceived(version);
            });

        return true;
    }

    bool CanSystemCategoryServer::HandleStatusRequest(infra::ConstByteRange)
    {
        NotifyObservers([](auto& observer)
            {
                observer.OnStatusRequest();
            });

        return true;
    }

    bool CanSystemCategoryServer::HandleCategoryListRequest(infra::ConstByteRange)
    {
        NotifyObservers([](auto& observer)
            {
                observer.OnCategoryListRequest();
            });

        return true;
    }
}
