#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSystemCategory.hpp"
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
    {
    public:
        struct Config
        {
            uint16_t nodeId;
            uint16_t maxMessagesPerSecond;
            infra::Duration heartbeatInterval = std::chrono::seconds(1);
        };

        CanProtocolServer(hal::Can& can, const Config& config);

        void RegisterCategory(CanCategory& category);
        void UnregisterCategory(CanCategory& category);

    private:
        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data);
        void SendCommandAck(uint8_t category, uint8_t commandType, CanAckStatus status);
        void SendHeartbeat();
        void SendCategoryList();
        bool CheckAndIncrementRate();
        void ResetRateCounter();
        bool ValidateSequence(uint8_t sequenceNumber);
        CanCategory* FindCategory(uint8_t categoryId);

        Config config;
        CanFrameTransport transport;
        infra::TimerRepeating heartbeatTimer;
        infra::TimerRepeating rateResetTimer;
        uint16_t messageCountThisPeriod = 0;
        uint8_t lastSequenceNumber = 0;
        bool sequenceInitialized = false;

        CanSystemCategory systemCategory;
        infra::IntrusiveList<CanCategory> categories;
    };
}
