#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include <cstdint>

namespace services
{
    class CanSystemCategoryClient
        : public CanCategoryClient
    {
    public:
        CanSystemCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource);

        uint8_t Id() const override;

        bool RequestCategoryList(uint16_t nodeId);

    private:
        bool HandleCommandAck(const hal::Can::Message& data);
        bool HandleCategoryListResponse(const hal::Can::Message& data);

        CanMessageHandler<CanSystemCategoryClient> commandAck{ canCommandAckMessageTypeId, *this, &CanSystemCategoryClient::HandleCommandAck };
        CanMessageHandler<CanSystemCategoryClient> categoryListResponse{ canCategoryListResponseMessageTypeId, *this, &CanSystemCategoryClient::HandleCategoryListResponse };
    };
}
