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
        : public infra::SingleObserver<CanProtocolServerObserver, CanProtocolServer>
    {
    public:
        using infra::SingleObserver<CanProtocolServerObserver, CanProtocolServer>::SingleObserver;

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
        };

        CanProtocolServer(hal::Can& can, const Config& config);

        bool RegisterCategory(CanCategoryServer& category);
        void UnregisterCategory(CanCategoryServer& category);

        void AttachIsoTpTransport(IsoTpTransport& isoTp);

        CanFrameTransport& Transport();

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

        struct SequenceValidationResult
        {
            bool accepted;
            uint8_t expected;
        };

        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data);
        void SendHeartbeat();
        void SendCategoryList();
        bool CheckAndIncrementRate();
        void ResetRateCounter();
        SequenceValidationResult ValidateSequence(uint8_t sequenceNumber);
        void SendCommandAck(uint8_t category, uint8_t commandType, CanAckStatus status, uint8_t expectedSequence);
        CanCategoryServer* FindCategory(uint8_t categoryId);
        void ResetHeartbeatTimer();
        void DispatchPdu(uint32_t rawId, infra::ConstByteRange pdu);

        Config config;
        CanFrameTransport transport;
        infra::TimerSingleShot heartbeatTimer;
        infra::TimerRepeating rateResetTimer;
        uint16_t messageCountThisPeriod = 0;
        uint8_t lastSequenceNumber = 0;
        bool sequenceInitialized = false;

        CanSystemCategoryServer systemCategory;
        SystemObserver systemObserver;
        infra::IntrusiveList<CanCategoryServer> categories;
        IsoTpTransport* isoTpTransport = nullptr;
    };
}
