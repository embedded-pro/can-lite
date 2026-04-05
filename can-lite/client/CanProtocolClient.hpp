#pragma once

#include "can-lite/categories/system/CanSystemCategoryClient.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/IntrusiveList.hpp"
#include "infra/util/Observer.hpp"
#include <array>
#include <cstdint>

namespace services
{
    class CanProtocolClient;

    class CanProtocolClientObserver
        : public infra::SingleObserver<CanProtocolClientObserver, CanProtocolClient>
    {
    public:
        using infra::SingleObserver<CanProtocolClientObserver, CanProtocolClient>::SingleObserver;

        virtual void OnServerOnline(uint16_t nodeId) = 0;
        virtual void OnServerOffline(uint16_t nodeId) = 0;
    };

    class CanProtocolClient
        : public infra::Subject<CanProtocolClientObserver>
    {
    public:
        struct Config
        {
            infra::Duration serverTimeout = std::chrono::seconds(3);
        };

        explicit CanProtocolClient(hal::Can& can);
        CanProtocolClient(hal::Can& can, const Config& config);

        CanProtocolClient(const CanProtocolClient&) = delete;
        CanProtocolClient& operator=(const CanProtocolClient&) = delete;
        CanProtocolClient(CanProtocolClient&&) = delete;
        CanProtocolClient& operator=(CanProtocolClient&&) = delete;

        void RegisterCategory(CanCategoryClient& category);
        void UnregisterCategory(CanCategoryClient& category);

        void DiscoverCategories(uint16_t nodeId, const infra::Function<void(const hal::Can::Message&)>& onDone);

        uint8_t PeekSequence(uint16_t nodeId);
        void CommitSequence(uint16_t nodeId);

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
        void MarkServerAlive(uint16_t nodeId);
        void HandleServerTimeout(uint16_t nodeId);

        struct PerServerState
        {
            uint16_t nodeId = 0;
            uint8_t sequenceCounter = 0;
            bool occupied = false;
        };

        struct ServerLiveness
        {
            uint16_t nodeId = 0;
            bool online = false;
            bool occupied = false;
            infra::TimerSingleShot timeoutTimer;
        };

        static constexpr uint8_t maxServers = 8;

        Config config;
        CanFrameTransport transport;
        CanSystemCategoryClient systemCategory;
        SystemObserver systemObserver;
        infra::IntrusiveList<CanCategoryClient> categories;
        infra::Function<void(const hal::Can::Message&)> pendingDiscoveryCallback;
        std::array<PerServerState, maxServers> serverStates;
        std::array<ServerLiveness, maxServers> serverLiveness;
    };
}
