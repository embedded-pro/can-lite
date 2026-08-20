#pragma once

#include "can-lite/categories/system/CanSystemCategoryClient.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceSource.hpp"
#include "can-lite/transport/IsoTpTransport.hpp"
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
        : public infra::Observer<CanProtocolClientObserver, CanProtocolClient>
    {
    public:
        using infra::Observer<CanProtocolClientObserver, CanProtocolClient>::Observer;

        virtual void OnServerOnline(uint16_t nodeId) = 0;
        virtual void OnServerOffline(uint16_t nodeId) = 0;
        virtual void OnCommandAckTimeout(uint16_t nodeId, uint8_t category, uint8_t messageType) = 0;
    };

    class CanProtocolClient
        : public infra::Subject<CanProtocolClientObserver>
        , public CanSequenceSource
    {
    public:
        struct Config
        {
            infra::Duration serverTimeout = std::chrono::seconds(3);
            infra::Duration heartbeatInterval = std::chrono::seconds(1);
            infra::Duration commandAckTimeout = std::chrono::seconds(1);
        };

        explicit CanProtocolClient(hal::Can& can);
        CanProtocolClient(hal::Can& can, const Config& config);

        CanProtocolClient(const CanProtocolClient&) = delete;
        CanProtocolClient& operator=(const CanProtocolClient&) = delete;
        CanProtocolClient(CanProtocolClient&&) = delete;
        CanProtocolClient& operator=(CanProtocolClient&&) = delete;

        bool RegisterCategory(CanCategoryClient& category);
        void UnregisterCategory(CanCategoryClient& category);

        CanSystemCategoryClient& SystemCategory();

        void DiscoverCategories(uint16_t nodeId, const infra::Function<void(const hal::Can::Message&)>& onDone);

        void AttachIsoTpTransport(IsoTpTransport& isoTp);

        CanFrameTransport& Transport();

        // CanSequenceSource
        uint8_t PeekSequence(uint16_t nodeId) override;
        void CommitSequence(uint16_t nodeId, uint8_t category, uint8_t messageType) override;

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
        void DispatchPdu(uint32_t rawId, infra::ConstByteRange pdu);
        void MarkServerAlive(uint16_t nodeId);
        void HandleServerTimeout(uint16_t nodeId);
        void HandleCommandAckFrame(uint16_t sourceNodeId, uint8_t categoryId, uint8_t messageType, infra::ConstByteRange payload);
        void ResyncSequence(uint16_t nodeId, uint8_t expectedSequence);
        void ClearAwaitingAck(uint16_t nodeId, uint8_t category, uint8_t messageType);
        void HandleCommandAckTimeout(uint16_t nodeId);
        void SendHeartbeat();
        void ResetHeartbeatTimer();

        struct PerServerState
        {
            uint16_t nodeId = 0;
            uint8_t sequenceCounter = 0;
            bool occupied = false;

            bool awaitingAck = false;
            uint8_t awaitingCategory = 0;
            uint8_t awaitingMessageType = 0;
            infra::TimerSingleShot ackTimer;
        };

        struct ServerLiveness
        {
            uint16_t nodeId = 0;
            bool occupied = false;
            infra::TimerSingleShot timeoutTimer;
        };

        static constexpr uint8_t maxServers = 8;

        Config config;
        CanFrameTransport transport;
        infra::TimerSingleShot heartbeatTimer;
        CanSystemCategoryClient systemCategory;
        SystemObserver systemObserver;
        infra::IntrusiveList<CanCategoryClient> categories;
        infra::Function<void(const hal::Can::Message&)> pendingDiscoveryCallback;
        std::array<PerServerState, maxServers> serverStates;
        std::array<ServerLiveness, maxServers> serverLiveness;
        uint8_t nextSequenceEvictIndex = 0;
        uint8_t nextLivenessEvictIndex = 0;
        IsoTpTransport* isoTpTransport = nullptr;
    };
}
