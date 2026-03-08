#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSystemCategory.hpp"
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

        void RegisterCategory(CanCategory& category);
        void UnregisterCategory(CanCategory& category);

        void DiscoverCategories(uint16_t nodeId, const infra::Function<void(const hal::Can::Message&)>& onDone);

    private:
        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data);

        CanFrameTransport transport;
        CanSystemCategory systemCategory;
        infra::IntrusiveList<CanCategory> categories;
        infra::Function<void(const hal::Can::Message&)> pendingDiscoveryCallback;
    };
}
