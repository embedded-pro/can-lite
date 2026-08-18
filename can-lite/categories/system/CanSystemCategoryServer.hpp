#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class CanSystemCategoryServer;

    class CanSystemCategoryServerObserver
        : public infra::SingleObserver<CanSystemCategoryServerObserver, CanSystemCategoryServer>
    {
    public:
        using infra::SingleObserver<CanSystemCategoryServerObserver, CanSystemCategoryServer>::SingleObserver;

        virtual void OnHeartbeatReceived(uint8_t version) = 0;
        virtual void OnStatusRequest() = 0;
        virtual void OnCategoryListRequest() = 0;
    };

    class CanSystemCategoryServer
        : public CanCategoryServer
        , public infra::Subject<CanSystemCategoryServerObserver>
    {
    public:
        explicit CanSystemCategoryServer(CanFrameTransport& transport);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

    private:
        void HandleHeartbeat(const hal::Can::Message& data);
        void HandleStatusRequest(const hal::Can::Message& data);
        void HandleCategoryListRequest(const hal::Can::Message& data);

        CanMessageHandler<CanSystemCategoryServer> heartbeat{ canHeartbeatMessageTypeId, *this, &CanSystemCategoryServer::HandleHeartbeat };
        CanMessageHandler<CanSystemCategoryServer> statusRequest{ canStatusRequestMessageTypeId, *this, &CanSystemCategoryServer::HandleStatusRequest };
        CanMessageHandler<CanSystemCategoryServer> categoryListRequest{ canCategoryListRequestMessageTypeId, *this, &CanSystemCategoryServer::HandleCategoryListRequest };
    };
}
