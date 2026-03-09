#include "can-lite/categories/system/CanSystemCategoryClient.hpp"

namespace services
{
    CanSystemCategoryClient::CanSystemCategoryClient()
        : commandAck(*this)
        , categoryListResponse(*this)
    {
        AddMessageType(commandAck);
        AddMessageType(categoryListResponse);
    }

    uint8_t CanSystemCategoryClient::Id() const
    {
        return canSystemCategoryId;
    }

    CanSystemCategoryClient::CommandAckMessageType::CommandAckMessageType(CanSystemCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategoryClient::CommandAckMessageType::Id() const
    {
        return canCommandAckMessageTypeId;
    }

    void CanSystemCategoryClient::CommandAckMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 3)
            return;

        if (parent.onCommandAck)
            parent.onCommandAck(data[0], data[1], static_cast<CanAckStatus>(data[2]));
    }

    CanSystemCategoryClient::CategoryListResponseMessageType::CategoryListResponseMessageType(CanSystemCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategoryClient::CategoryListResponseMessageType::Id() const
    {
        return canCategoryListResponseMessageTypeId;
    }

    void CanSystemCategoryClient::CategoryListResponseMessageType::Handle(const hal::Can::Message& data)
    {
        parent.NotifyObservers([&data](auto& observer)
            {
                observer.OnCategoryListResponse(data);
            });
    }
}
