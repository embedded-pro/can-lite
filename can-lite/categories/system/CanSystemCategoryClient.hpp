#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class CanSystemCategoryClient;

    class CanSystemCategoryClientObserver
        : public infra::SingleObserver<CanSystemCategoryClientObserver, CanSystemCategoryClient>
    {
    public:
        using infra::SingleObserver<CanSystemCategoryClientObserver, CanSystemCategoryClient>::SingleObserver;

        virtual void OnCategoryListResponse(infra::ConstByteRange categoryIds) = 0;
    };

    class CanSystemCategoryClient
        : private CanCategoryHandlerStorage<2>
        , public CanCategoryClient
        , public infra::Subject<CanSystemCategoryClientObserver>
    {
    public:
        CanSystemCategoryClient();

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

        infra::Function<void(uint8_t category, uint8_t command, CanAckStatus status)> onCommandAck;

    private:
        bool HandleCommandAck(infra::ConstByteRange payload);
        bool HandleCategoryListResponse(infra::ConstByteRange payload);
    };
}
