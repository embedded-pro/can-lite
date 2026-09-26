#include "can-lite/categories/system/CanSystemCategoryServer.hpp"

namespace services
{
    CanSystemCategoryServer::CanSystemCategoryServer(CanFrameTransport& transport)
        : CanCategoryServer(transport)
    {
        AddMessageTypes(heartbeat, statusRequest, categoryListRequest);
    }

    uint8_t CanSystemCategoryServer::Id() const
    {
        return canSystemCategoryId;
    }

    bool CanSystemCategoryServer::RequiresSequenceValidation() const
    {
        return false;
    }

    bool CanSystemCategoryServer::HandleHeartbeat(const hal::Can::Message& data)
    {
        auto version = data.empty() ? uint8_t{ 0 } : data[0];

        NotifyObservers([version](auto& observer)
            {
                observer.OnHeartbeatReceived(version);
            });

        return true;
    }

    bool CanSystemCategoryServer::HandleStatusRequest(const hal::Can::Message&)
    {
        NotifyObservers([](auto& observer)
            {
                observer.OnStatusRequest();
            });

        return true;
    }

    bool CanSystemCategoryServer::HandleCategoryListRequest(const hal::Can::Message&)
    {
        NotifyObservers([](auto& observer)
            {
                observer.OnCategoryListRequest();
            });

        return true;
    }
}
