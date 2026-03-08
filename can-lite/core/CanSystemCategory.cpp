#include "can-lite/core/CanSystemCategory.hpp"

namespace services
{
    CanSystemCategory::CanSystemCategory()
        : heartbeat(*this)
        , statusRequest(*this)
        , commandAck(*this)
        , categoryListRequest(*this)
        , categoryListResponse(*this)
    {
        AddMessageType(heartbeat);
        AddMessageType(statusRequest);
        AddMessageType(commandAck);
        AddMessageType(categoryListRequest);
        AddMessageType(categoryListResponse);
    }

    uint8_t CanSystemCategory::Id() const
    {
        return canSystemCategoryId;
    }

    bool CanSystemCategory::RequiresSequenceValidation() const
    {
        return false;
    }

    CanSystemCategory::HeartbeatMessageType::HeartbeatMessageType(CanSystemCategory& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategory::HeartbeatMessageType::Id() const
    {
        return canHeartbeatMessageTypeId;
    }

    void CanSystemCategory::HeartbeatMessageType::Handle(const hal::Can::Message& data)
    {
        if (parent.onHeartbeat)
            parent.onHeartbeat(data.empty() ? 0 : data[0]);
    }

    CanSystemCategory::StatusRequestMessageType::StatusRequestMessageType(CanSystemCategory& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategory::StatusRequestMessageType::Id() const
    {
        return canStatusRequestMessageTypeId;
    }

    void CanSystemCategory::StatusRequestMessageType::Handle(const hal::Can::Message& data)
    {
        if (parent.onStatusRequest)
            parent.onStatusRequest();
    }

    CanSystemCategory::CommandAckMessageType::CommandAckMessageType(CanSystemCategory& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategory::CommandAckMessageType::Id() const
    {
        return canCommandAckMessageTypeId;
    }

    void CanSystemCategory::CommandAckMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 3)
            return;

        if (parent.onCommandAck)
            parent.onCommandAck(data[0], data[1], static_cast<CanAckStatus>(data[2]));
    }

    CanSystemCategory::CategoryListRequestMessageType::CategoryListRequestMessageType(CanSystemCategory& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategory::CategoryListRequestMessageType::Id() const
    {
        return canCategoryListRequestMessageTypeId;
    }

    void CanSystemCategory::CategoryListRequestMessageType::Handle(const hal::Can::Message& data)
    {
        if (parent.onCategoryListRequest)
            parent.onCategoryListRequest();
    }

    CanSystemCategory::CategoryListResponseMessageType::CategoryListResponseMessageType(CanSystemCategory& parent)
        : parent(parent)
    {}

    uint8_t CanSystemCategory::CategoryListResponseMessageType::Id() const
    {
        return canCategoryListResponseMessageTypeId;
    }

    void CanSystemCategory::CategoryListResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (parent.onCategoryListResponse)
            parent.onCategoryListResponse(data);
    }
}
