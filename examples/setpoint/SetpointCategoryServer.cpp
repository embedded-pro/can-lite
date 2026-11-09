#include "examples/setpoint/SetpointCategoryServer.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace services
{
    namespace
    {
        CanPayloadWriter StatusPayload(const SetpointStatus& status)
        {
            CanPayloadWriter payload;
            payload.WriteFixed16(status.setpoint, setpointValueScale)
                .WriteFixed16(status.measurement, setpointValueScale);
            return payload;
        }
    }

    SetpointCategoryServer::SetpointCategoryServer(CanFrameTransport& transport)
        : CanCategoryServer(transport)
    {
        AddMessageTypes(set, requestStatus);
    }

    uint8_t SetpointCategoryServer::Id() const
    {
        return setpointCategoryId;
    }

    void SetpointCategoryServer::PublishStatus(const SetpointStatus& status)
    {
        SendTelemetry(setpointStatusId, StatusPayload(status));
    }

    bool SetpointCategoryServer::HandleSet(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto setpoint = reader.ReadFixed16(setpointValueScale);
        if (!reader.Valid())
            return false;

        NotifyObservers([this, setpoint](auto& observer)
            {
                observer.OnSetSetpoint(setpoint, [this](CanAckStatus status)
                    {
                        SendCommandAck(setpointSetId, status);
                    });
            });

        return true;
    }

    bool SetpointCategoryServer::HandleRequestStatus(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnRequestStatus([this](SetpointStatus status)
                    {
                        SendResponse(setpointStatusId, StatusPayload(status));
                        SendCommandAck(setpointRequestStatusId, CanAckStatus::success);
                    });
            });

        return true;
    }
}
