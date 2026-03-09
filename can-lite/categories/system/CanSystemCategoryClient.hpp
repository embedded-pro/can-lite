#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageType.hpp"
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
        CanSystemCategoryClient();

        uint8_t Id() const override;

        infra::Function<void(uint8_t category, uint8_t command, CanAckStatus status)> onCommandAck;

    private:
        class CommandAckMessageType
            : public CanMessageType
        {
        public:
            explicit CommandAckMessageType(CanSystemCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategoryClient& parent;
        };

        class CategoryListResponseMessageType
            : public CanMessageType
        {
        public:
            explicit CategoryListResponseMessageType(CanSystemCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategoryClient& parent;
        };

        CommandAckMessageType commandAck;
        CategoryListResponseMessageType categoryListResponse;
    };
}
