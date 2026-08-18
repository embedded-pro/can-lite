#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class FirmwareUpgradeCategoryServer;

    class FirmwareUpgradeCategoryServerObserver
        : public infra::SingleObserver<FirmwareUpgradeCategoryServerObserver, FirmwareUpgradeCategoryServer>
    {
    public:
        using infra::SingleObserver<FirmwareUpgradeCategoryServerObserver, FirmwareUpgradeCategoryServer>::SingleObserver;

        virtual void OnBeginUpgrade(uint32_t firmwareSize, const infra::Function<void(FwuError, uint16_t)>& onResult) = 0;
        virtual void OnDataBlock(uint16_t blockIndex, const hal::Can::Message& data, const infra::Function<void(FwuError)>& onResult) = 0;
        virtual void OnVerify(uint32_t expectedCrc32, const infra::Function<void(FwuError)>& onResult) = 0;
        virtual void OnActivate(const infra::Function<void(FwuError)>& onResult) = 0;
        virtual void OnAbort(const infra::Function<void()>& onDone) = 0;
        virtual void OnQueryProgress(const infra::Function<void(FwuState, uint16_t, uint16_t)>& onResult) = 0;
        virtual void OnSessionTimeout() = 0;
    };

    class FirmwareUpgradeCategoryServer
        : public CanCategoryServer
        , public infra::Subject<FirmwareUpgradeCategoryServerObserver>
    {
    public:
        struct Config
        {
            infra::Duration sessionTimeout = std::chrono::seconds(30);
        };

        FirmwareUpgradeCategoryServer(CanFrameTransport& transport, const Config& config);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

    private:
        void SendBeginResponse(FwuError status, uint16_t pageSize);
        void SendDataBlockAck(FwuError status, uint16_t blockIndex);
        void SendVerifyResponse(FwuError status);
        void SendActivateResponse(FwuError status);
        void SendProgressResponse(FwuState state, uint16_t blocksReceived, uint16_t totalBlocks);

        void HandleBeginUpgrade(const hal::Can::Message& data);
        void HandleDataBlock(const hal::Can::Message& data);
        void HandleVerify(const hal::Can::Message& data);
        void HandleActivate(const hal::Can::Message& data);
        void HandleAbort(const hal::Can::Message& data);
        void HandleQueryProgress(const hal::Can::Message& data);

        void ResetSessionTimer();
        void StopSessionTimer();
        void HandleSessionTimeout();

        Config config;
        infra::TimerSingleShot sessionTimeoutTimer;

        CanMessageHandler<FirmwareUpgradeCategoryServer> beginUpgrade{ fwuBeginUpgradeId, *this, &FirmwareUpgradeCategoryServer::HandleBeginUpgrade };
        CanMessageHandler<FirmwareUpgradeCategoryServer> dataBlock{ fwuDataBlockId, *this, &FirmwareUpgradeCategoryServer::HandleDataBlock };
        CanMessageHandler<FirmwareUpgradeCategoryServer> verify{ fwuVerifyId, *this, &FirmwareUpgradeCategoryServer::HandleVerify };
        CanMessageHandler<FirmwareUpgradeCategoryServer> activate{ fwuActivateId, *this, &FirmwareUpgradeCategoryServer::HandleActivate };
        CanMessageHandler<FirmwareUpgradeCategoryServer> abort{ fwuAbortId, *this, &FirmwareUpgradeCategoryServer::HandleAbort };
        CanMessageHandler<FirmwareUpgradeCategoryServer> queryProgress{ fwuQueryProgressId, *this, &FirmwareUpgradeCategoryServer::HandleQueryProgress };
    };
}
