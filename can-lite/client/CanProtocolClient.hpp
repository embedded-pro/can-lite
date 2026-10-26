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
#include <optional>

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
            infra::Duration discoveryTimeout = std::chrono::seconds(1);
        };

        explicit CanProtocolClient(hal::Can& can);
        CanProtocolClient(hal::Can& can, const Config& config);

        CanProtocolClient(const CanProtocolClient&) = delete;
        CanProtocolClient& operator=(const CanProtocolClient&) = delete;
        CanProtocolClient(CanProtocolClient&&) = delete;
        CanProtocolClient& operator=(CanProtocolClient&&) = delete;

        ~CanProtocolClient();

        bool RegisterCategory(CanCategoryClient& category);
        void UnregisterCategory(CanCategoryClient& category);

        CanSystemCategoryClient& SystemCategory();

        bool DiscoverCategories(uint16_t nodeId, const infra::Function<void(std::optional<infra::ConstByteRange> categoryIds)>& onDone);

        void AttachIsoTpTransport(IsoTpTransport& isoTp);
        void DetachIsoTpTransport();

        CanFrameTransport& Transport();
        const CanFrameTransport& Transport() const;

        uint8_t ReserveSequence(uint16_t nodeId, uint8_t category, uint8_t messageType) override;
        void CancelReservation(uint16_t nodeId, uint8_t sequence) override;
        void ReturnSequence(uint16_t nodeId, uint8_t sequence) override;

    private:
        struct PerServerState;

        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data);
        void DispatchPdu(uint32_t rawId, infra::ConstByteRange pdu);
        void MarkServerAlive(uint16_t nodeId);
        void HandleServerTimeout(uint16_t nodeId);
        void HandleCommandAckFrame(uint16_t sourceNodeId, uint8_t categoryId, uint8_t messageType, infra::ConstByteRange payload);
        void ResyncSequence(uint16_t nodeId, uint8_t expectedSequence);
        void ClearAwaitingAck(uint16_t nodeId, uint8_t category, uint8_t messageType);
        void HandleCommandAckTimeout(uint16_t nodeId);
        void HandleCategoryListFrame(uint16_t sourceNodeId, uint8_t categoryId, uint8_t messageType, infra::ConstByteRange payload);
        void CompleteDiscovery(std::optional<infra::ConstByteRange> categoryIds);
        PerServerState& ClaimSequenceSlot(uint16_t nodeId);
        void AwaitAck(PerServerState& state, infra::TimePoint deadline);
        void SendHeartbeat();
        void ResetHeartbeatTimer();

        struct SequenceRecord
        {
            uint16_t nodeId = 0;
            uint8_t sequenceCounter = 0;
            bool occupied = false;

            bool awaitingAck = false;
            uint8_t awaitingCategory = 0;
            uint8_t awaitingMessageType = 0;
        };

        struct PerServerState
            : SequenceRecord
        {
            infra::TimerSingleShot ackTimer;
        };

        struct Reservation
        {
            PerServerState* state = nullptr;
            uint8_t sequence = 0;
            SequenceRecord previous;
            infra::TimePoint previousAckDeadline;
            uint8_t previousEvictIndex = 0;
        };

        struct ServerLiveness
        {
            uint16_t nodeId = 0;
            bool occupied = false;
            infra::TimerSingleShot timeoutTimer;
        };

        static constexpr uint8_t maxServers = 8;

        hal::Can& can;
        Config config;
        CanFrameTransport transport;
        infra::TimerSingleShot heartbeatTimer;
        CanSystemCategoryClient systemCategory;
        infra::IntrusiveList<CanCategoryClient> categories;
        std::optional<uint16_t> discoveryNode;
        infra::Function<void(std::optional<infra::ConstByteRange> categoryIds)> discoveryDone;
        infra::TimerSingleShot discoveryTimer;
        std::array<PerServerState, maxServers> serverStates;
        std::array<ServerLiveness, maxServers> serverLiveness;
        uint8_t nextSequenceEvictIndex = 0;
        std::optional<Reservation> lastReservation;
        uint8_t nextLivenessEvictIndex = 0;
        IsoTpTransport* isoTpTransport = nullptr;
    };
}
