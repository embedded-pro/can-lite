#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageType.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace services
{
    class CanSystemCategory
        : public CanCategory
    {
    public:
        CanSystemCategory();

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

        infra::Function<void(uint8_t version)> onHeartbeat;
        infra::Function<void()> onStatusRequest;
        infra::Function<void(uint8_t category, uint8_t command, CanAckStatus status)> onCommandAck;
        infra::Function<void()> onCategoryListRequest;
        infra::Function<void(const hal::Can::Message& categoryIds)> onCategoryListResponse;

    private:
        class HeartbeatMessageType
            : public CanMessageType
        {
        public:
            explicit HeartbeatMessageType(CanSystemCategory& parent);

            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategory& parent;
        };

        class StatusRequestMessageType
            : public CanMessageType
        {
        public:
            explicit StatusRequestMessageType(CanSystemCategory& parent);

            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategory& parent;
        };

        class CommandAckMessageType
            : public CanMessageType
        {
        public:
            explicit CommandAckMessageType(CanSystemCategory& parent);

            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategory& parent;
        };

        class CategoryListRequestMessageType
            : public CanMessageType
        {
        public:
            explicit CategoryListRequestMessageType(CanSystemCategory& parent);

            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategory& parent;
        };

        class CategoryListResponseMessageType
            : public CanMessageType
        {
        public:
            explicit CategoryListResponseMessageType(CanSystemCategory& parent);

            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            CanSystemCategory& parent;
        };

        HeartbeatMessageType heartbeat;
        StatusRequestMessageType statusRequest;
        CommandAckMessageType commandAck;
        CategoryListRequestMessageType categoryListRequest;
        CategoryListResponseMessageType categoryListResponse;
    };
}
