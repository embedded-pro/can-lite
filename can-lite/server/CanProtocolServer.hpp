#pragma once

#include "can-lite/categories/system/CanSystemCategoryServer.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/transport/IsoTpTransport.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/IntrusiveList.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class CanProtocolServer;

    class CanProtocolServerObserver
        : public infra::Observer<CanProtocolServerObserver, CanProtocolServer>
    {
    public:
        using infra::Observer<CanProtocolServerObserver, CanProtocolServer>::Observer;

        virtual void Online() = 0;
        virtual void Offline() = 0;
    };

    class CanProtocolServer
        : public infra::Subject<CanProtocolServerObserver>
        , public CanCommandAcknowledger
    {
    public:
        struct Config
        {
            uint16_t nodeId{ 0 };
            uint16_t maxMessagesPerSecond{ 500 };
            infra::Duration heartbeatInterval = std::chrono::seconds(1);
            infra::Duration clientTimeout = std::chrono::seconds(3);
            uint16_t maxEmergencyMessagesPerSecond{ 20 };
        };

        struct Counters
        {
            uint32_t rateLimited{};
            uint32_t emergencyRateLimited{};
            uint32_t invalidFrames{};
            uint32_t emergencyAdmitted{};
        };

        CanProtocolServer(hal::Can& can, const Config& config);
        CanProtocolServer(const CanProtocolServer&) = delete;
        CanProtocolServer(CanProtocolServer&&) = delete;
        CanProtocolServer& operator=(const CanProtocolServer&) = delete;
        CanProtocolServer& operator=(CanProtocolServer&&) = delete;
        ~CanProtocolServer();

        bool RegisterCategory(CanCategoryServer& category);
        bool UnregisterCategory(CanCategoryServer& category);

        void AttachIsoTpTransport(IsoTpTransport& isoTp);
        void DetachIsoTpTransport();

        CanFrameTransport& Transport();
        const Counters& Statistics() const;

        // CanCommandAcknowledger
        void SendCommandAck(uint8_t category, uint8_t commandType, CanAckStatus status) override;

    private:
        class SystemObserver
            : public CanSystemCategoryServerObserver
        {
        public:
            SystemObserver(CanSystemCategoryServer& subject, CanProtocolServer& server);

            void OnHeartbeatReceived(uint8_t version) override;
            void OnStatusRequest() override;
            void OnCategoryListRequest() override;

        private:
            CanProtocolServer& server;
        };

        struct SequenceCheckpoint
        {
            uint8_t lastSequenceNumber;
            bool sequenceInitialized;
        };

        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data);
        void DispatchPdu(uint32_t rawId, infra::ConstByteRange pdu);
        bool IsAddressedToThisNode(uint32_t rawId) const;
        bool CheckAndIncrementRate(uint32_t rawId);
        void ResetRateCounter();
        CanCategoryServer* AdmitCommand(uint32_t rawId, infra::ConstByteRange payload);
        bool IsExpectedSequence(uint8_t sequenceNumber) const;
        uint8_t ExpectedSequence() const;
        SequenceCheckpoint CommitSequence(const CanCategoryServer& category, infra::ConstByteRange payload);
        void RestoreSequence(const SequenceCheckpoint& checkpoint);
        void Conclude(uint32_t rawId, const SequenceCheckpoint& checkpoint, CanDispatchResult result);
        void AcceptCommand(uint32_t rawId);
        void RejectCommand(uint32_t rawId, CanAckStatus status, uint8_t expectedSequence);
        void SendHeartbeat();
        void SendCategoryList();
        void SendCommandAck(uint8_t category, uint8_t commandType, CanAckStatus status, uint8_t expectedSequence);
        CanCategoryServer* FindCategory(uint8_t categoryId);
        void ResetHeartbeatTimer();
        void MarkClientAlive();
        void HandleClientTimeout();

        hal::Can& can;
        Config config;
        CanFrameTransport transport;
        infra::TimerSingleShot heartbeatTimer;
        infra::TimerRepeating rateResetTimer;
        infra::TimerSingleShot clientLivenessTimer;
        uint16_t messageCountThisPeriod = 0;
        uint16_t emergencyMessageCountThisPeriod = 0;
        Counters counters;
        uint8_t lastSequenceNumber = 0;
        bool sequenceInitialized = false;
        bool clientOnline = false;

        CanSystemCategoryServer systemCategory;
        SystemObserver systemObserver;
        infra::IntrusiveList<CanCategoryServer> categories;
        IsoTpTransport* isoTpTransport = nullptr;
    };
}
