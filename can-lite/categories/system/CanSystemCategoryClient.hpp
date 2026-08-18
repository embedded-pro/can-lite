#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
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

        virtual void OnCategoryListResponse(const hal::Can::Message& categoryIds) = 0;
    };

    class CanSystemCategoryClient
        : public CanCategoryClient
        , public infra::Subject<CanSystemCategoryClientObserver>
    {
    public:
        CanSystemCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource);

        uint8_t Id() const override;

        infra::Function<void(uint8_t category, uint8_t command, CanAckStatus status)> onCommandAck;

    private:
        void HandleCommandAck(const hal::Can::Message& data);
        void HandleCategoryListResponse(const hal::Can::Message& data);

        CanMessageHandler<CanSystemCategoryClient> commandAck{ canCommandAckMessageTypeId, *this, &CanSystemCategoryClient::HandleCommandAck };
        CanMessageHandler<CanSystemCategoryClient> categoryListResponse{ canCategoryListResponseMessageTypeId, *this, &CanSystemCategoryClient::HandleCategoryListResponse };
    };
}
