#include "can-lite/categories/system/CanSystemCategoryClient.hpp"
#include "can-lite/core/CanPayload.hpp"

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

    void CanSystemCategoryClient::HandleCommandAck(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto category = reader.ReadUInt8();
        auto command = reader.ReadUInt8();
        auto status = static_cast<CanAckStatus>(reader.ReadUInt8());

        if (!reader.Valid())
            return;

        if (onCommandAck)
            onCommandAck(category, command, status);
    }

    void CanSystemCategoryClient::HandleCategoryListResponse(const hal::Can::Message& data)
    {
        NotifyObservers([&data](auto& observer)
            {
                observer.OnCategoryListResponse(data);
            });
    }
}
