#pragma once

#include "can-lite/core/CanCategory.hpp"
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
        : private CanCategoryHandlerStorage<3>
        , public CanCategoryServer
        , public infra::Subject<CanSystemCategoryServerObserver>
    {
    public:
        CanSystemCategoryServer();

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

    private:
        bool HandleHeartbeat(infra::ConstByteRange payload);
        bool HandleStatusRequest(infra::ConstByteRange payload);
        bool HandleCategoryListRequest(infra::ConstByteRange payload);
    };
}
