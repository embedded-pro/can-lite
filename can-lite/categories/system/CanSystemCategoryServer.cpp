#include "can-lite/categories/system/CanSystemCategoryServer.hpp"

namespace services
{
    CanSystemCategoryServer::CanSystemCategoryServer()
        : heartbeat(*this)
        , statusRequest(*this)
        , categoryListRequest(*this)
    {
        AddMessageType(heartbeat);
        AddMessageType(statusRequest);
        AddMessageType(categoryListRequest);
    }

    uint8_t CanSystemCategoryServer::Id() const
    {
        return canSystemCategoryId;
    }

    bool CanSystemCategoryServer::RequiresSequenceValidation() const
    {
        return false;
    }

    CanSystemCategoryServer::HeartbeatMessageType::HeartbeatMessageType(CanSystemCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategoryServer::HeartbeatMessageType::Id() const
    {
        return canHeartbeatMessageTypeId;
    }

    void CanSystemCategoryServer::HeartbeatMessageType::Handle(const hal::Can::Message& data)
    {
        auto version = data.empty() ? uint8_t{ 0 } : data[0];

        parent.NotifyObservers([version](auto& observer)
            {
                observer.OnHeartbeatReceived(version);
            });
    }

    CanSystemCategoryServer::StatusRequestMessageType::StatusRequestMessageType(CanSystemCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategoryServer::StatusRequestMessageType::Id() const
    {
        return canStatusRequestMessageTypeId;
    }

    void CanSystemCategoryServer::StatusRequestMessageType::Handle(const hal::Can::Message& data)
    {
        parent.NotifyObservers([](auto& observer)
            {
                observer.OnStatusRequest();
            });
    }

    CanSystemCategoryServer::CategoryListRequestMessageType::CategoryListRequestMessageType(CanSystemCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategoryServer::CategoryListRequestMessageType::Id() const
    {
        return canCategoryListRequestMessageTypeId;
    }

    void CanSystemCategoryServer::CategoryListRequestMessageType::Handle(const hal::Can::Message& data)
    {
        parent.NotifyObservers([](auto& observer)
            {
                observer.OnCategoryListRequest();
            });
    }
}
