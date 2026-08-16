#pragma once

#include "can-lite/categories/system/CanSystemCategoryServer.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanCategoryOutbound.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/transport/IsoTpTransport.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/IntrusiveList.hpp"
#include "infra/util/Observer.hpp"
#include <array>
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

        void RegisterCategory(CanCategoryServer& category);
        void UnregisterCategory(CanCategoryServer& category);

        void AttachIsoTpTransport(IsoTpTransport& isoTp);

        CanFrameTransport& Transport();

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

        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data);
        void Dispatch(uint32_t rawId, infra::ConstByteRange payload);
        void SendHeartbeat();
        void SendCategoryList();
        bool CheckAndIncrementRate();
        void ResetRateCounter();
        CanCategoryServer* FindCategory(uint8_t categoryId);
        CanCategoryOutboundImpl* FindOutbound(uint8_t categoryId);
        CanCategoryOutboundImpl& AllocateOutbound(uint8_t categoryId);
        void ResetHeartbeatTimer();

        Config config;
        CanFrameTransport transport;
        infra::TimerSingleShot heartbeatTimer;
        infra::TimerRepeating rateResetTimer;
        uint16_t messageCountThisPeriod = 0;

        CanSystemCategoryServer systemCategory;
        SystemObserver systemObserver;
        infra::IntrusiveList<CanCategoryServer> categories;
        uint8_t categoryCount = 0;
        std::array<CanCategoryOutboundImpl, canMaxCategories> outbounds;
        IsoTpTransport* isoTpTransport = nullptr;
    };
}
