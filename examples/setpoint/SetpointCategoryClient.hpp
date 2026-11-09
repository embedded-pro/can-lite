#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "examples/setpoint/SetpointDefinitions.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class SetpointCategoryClient;

    class SetpointCategoryClientObserver
        : public infra::SingleObserver<SetpointCategoryClientObserver, SetpointCategoryClient>
    {
    public:
        using infra::SingleObserver<SetpointCategoryClientObserver, SetpointCategoryClient>::SingleObserver;

        virtual void OnStatus(const SetpointStatus& status) = 0;
    };

    class SetpointCategoryClient
        : public CanCategoryClient
        , public infra::Subject<SetpointCategoryClientObserver>
    {
    public:
        SetpointCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource);

        uint8_t Id() const override;

        bool SendSetSetpoint(uint16_t targetNodeId, float setpoint);
        bool SendRequestStatus(uint16_t targetNodeId);

    private:
        bool HandleStatus(const hal::Can::Message& data);

        CanMessageHandler<SetpointCategoryClient> status{ setpointStatusId, *this, &SetpointCategoryClient::HandleStatus };
    };
}
