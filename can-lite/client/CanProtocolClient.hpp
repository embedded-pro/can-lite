#pragma once

#include "can-lite/categories/system/CanSystemCategoryClient.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/IntrusiveList.hpp"
#include <cstdint>

namespace services
{
    class CanProtocolClient
    {
    public:
        CanProtocolClient(hal::Can& can);

        CanProtocolClient(const CanProtocolClient&) = delete;
        CanProtocolClient& operator=(const CanProtocolClient&) = delete;
        CanProtocolClient(CanProtocolClient&&) = delete;
        CanProtocolClient& operator=(CanProtocolClient&&) = delete;

        void RegisterCategory(CanCategoryClient& category);
        void UnregisterCategory(CanCategoryClient& category);

        void DiscoverCategories(uint16_t nodeId, const infra::Function<void(const hal::Can::Message&)>& onDone);

    private:
        class SystemObserver
            : public CanSystemCategoryClientObserver
        {
        public:
            SystemObserver(CanSystemCategoryClient& subject, CanProtocolClient& client);

            void OnCategoryListResponse(const hal::Can::Message& categoryIds) override;

        private:
            CanProtocolClient& client;
        };

        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data);

        CanFrameTransport transport;
        CanSystemCategoryClient systemCategory;
        SystemObserver systemObserver;
        infra::IntrusiveList<CanCategoryClient> categories;
        infra::Function<void(const hal::Can::Message&)> pendingDiscoveryCallback;
    };
}
