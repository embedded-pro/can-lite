#include "can-lite/categories/system/CanSystemCategoryClient.hpp"

namespace services
{
    CanSystemCategoryClient::CanSystemCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource)
        : CanCategoryClient(transport, sequenceSource)
    {
        AddMessageTypes(commandAck, categoryListResponse);
    }

    uint8_t CanSystemCategoryClient::Id() const
    {
        return canSystemCategoryId;
    }

    bool CanSystemCategoryClient::RequestCategoryList(uint16_t nodeId)
    {
        return SendCommandWithoutSequence(nodeId, canCategoryListRequestMessageTypeId);
    }

    bool CanSystemCategoryClient::HandleCommandAck(const hal::Can::Message&)
    {
        return true;
    }

    bool CanSystemCategoryClient::HandleCategoryListResponse(const hal::Can::Message&)
    {
        return true;
    }
}
