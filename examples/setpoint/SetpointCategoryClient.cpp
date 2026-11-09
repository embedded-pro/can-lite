#include "examples/setpoint/SetpointCategoryClient.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace services
{
    SetpointCategoryClient::SetpointCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource)
        : CanCategoryClient(transport, sequenceSource)
    {
        AddMessageTypes(status);
    }

    uint8_t SetpointCategoryClient::Id() const
    {
        return setpointCategoryId;
    }

    bool SetpointCategoryClient::SendSetSetpoint(uint16_t targetNodeId, float setpoint)
    {
        CanPayloadWriter payload;
        payload.WriteFixed16(setpoint, setpointValueScale);
        return SendCommand(targetNodeId, setpointSetId, payload);
    }

    bool SetpointCategoryClient::SendRequestStatus(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, setpointRequestStatusId);
    }

    bool SetpointCategoryClient::HandleStatus(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        SetpointStatus received{ reader.ReadFixed16(setpointValueScale), reader.ReadFixed16(setpointValueScale) };
        if (!reader.Valid())
            return false;

        NotifyObservers([&received](auto& observer)
            {
                observer.OnStatus(received);
            });

        return true;
    }
}
