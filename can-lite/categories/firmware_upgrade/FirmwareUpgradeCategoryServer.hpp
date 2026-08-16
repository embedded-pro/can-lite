#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/ByteRange.hpp"
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
        virtual void OnDataBlock(uint16_t blockIndex, infra::ConstByteRange data, const infra::Function<void(FwuError)>& onResult) = 0;
        virtual void OnVerify(uint32_t expectedCrc32, const infra::Function<void(FwuError)>& onResult) = 0;
        virtual void OnActivate(const infra::Function<void(FwuError)>& onResult) = 0;
        virtual void OnAbort(const infra::Function<void()>& onDone) = 0;
        virtual void OnQueryProgress(const infra::Function<void(FwuState, uint16_t, uint16_t)>& onResult) = 0;
        virtual void OnSessionTimeout() = 0;
    };

    class FirmwareUpgradeCategoryServer
        : private CanCategoryHandlerStorage<6>
        , public CanCategoryServer
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
        bool HandleBeginUpgrade(infra::ConstByteRange payload);
        bool HandleDataBlock(infra::ConstByteRange payload);
        bool HandleVerify(infra::ConstByteRange payload);
        bool HandleActivate(infra::ConstByteRange payload);
        bool HandleAbort(infra::ConstByteRange payload);
        bool HandleQueryProgress(infra::ConstByteRange payload);

        void SendBeginResponse(FwuError status, uint16_t pageSize);
        void SendDataBlockAck(FwuError status, uint16_t blockIndex);
        void SendVerifyResponse(FwuError status);
        void SendActivateResponse(FwuError status);
        void SendProgressResponse(FwuState state, uint16_t blocksReceived, uint16_t totalBlocks);

        void ResetSessionTimer();
        void StopSessionTimer();
        void HandleSessionTimeout();

        CanFrameTransport& transport;
        Config config;
        infra::TimerSingleShot sessionTimeoutTimer;
    };
}
