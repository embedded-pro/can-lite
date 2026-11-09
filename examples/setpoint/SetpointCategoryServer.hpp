#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "examples/setpoint/SetpointDefinitions.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class SetpointCategoryServer;

    class SetpointCategoryServerObserver
        : public infra::SingleObserver<SetpointCategoryServerObserver, SetpointCategoryServer>
    {
    public:
        using infra::SingleObserver<SetpointCategoryServerObserver, SetpointCategoryServer>::SingleObserver;

        virtual void OnSetSetpoint(float setpoint, const infra::Function<void(CanAckStatus)>& onDone) = 0;
        virtual void OnRequestStatus(const infra::Function<void(SetpointStatus)>& onResult) = 0;
    };

    class SetpointCategoryServer
        : public CanCategoryServer
        , public infra::Subject<SetpointCategoryServerObserver>
    {
    public:
        explicit SetpointCategoryServer(CanFrameTransport& transport);

        uint8_t Id() const override;

        void PublishStatus(const SetpointStatus& status);

    private:
        bool HandleSet(const hal::Can::Message& data);
        bool HandleRequestStatus(const hal::Can::Message& data);

        CanMessageHandler<SetpointCategoryServer> set{ setpointSetId, *this, &SetpointCategoryServer::HandleSet };
        CanMessageHandler<SetpointCategoryServer> requestStatus{ setpointRequestStatusId, *this, &SetpointCategoryServer::HandleRequestStatus };
    };
}
