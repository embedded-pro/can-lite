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

    void CanSystemCategoryClient::HandleCommandAck(const hal::Can::Message&)
    {
        // Acknowledged but currently not surfaced to observers; command
        // completion is tracked application-side via each category's own
        // response/telemetry frames.
    }

    void CanSystemCategoryClient::HandleCategoryListResponse(const hal::Can::Message& data)
    {
        NotifyObservers([&data](auto& observer)
            {
                observer.OnCategoryListResponse(data);
            });
    }
}
