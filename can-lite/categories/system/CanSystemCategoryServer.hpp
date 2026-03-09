#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageType.hpp"
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
        CanSystemCategoryServer();

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

    private:
        class HeartbeatMessageType
            : public CanMessageType
        {
        public:
            explicit HeartbeatMessageType(CanSystemCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategoryServer& parent;
        };

        class StatusRequestMessageType
            : public CanMessageType
        {
        public:
            explicit StatusRequestMessageType(CanSystemCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategoryServer& parent;
        };

        class CategoryListRequestMessageType
            : public CanMessageType
        {
        public:
            explicit CategoryListRequestMessageType(CanSystemCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategoryServer& parent;
        };

        HeartbeatMessageType heartbeat;
        StatusRequestMessageType statusRequest;
        CategoryListRequestMessageType categoryListRequest;
    };
}
