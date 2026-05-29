#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageType.hpp"
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

        class BeginUpgradeMessageType
            : public CanMessageType
        {
        public:
            explicit BeginUpgradeMessageType(FirmwareUpgradeCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryServer& parent;
        };

        class DataBlockMessageType
            : public CanMessageType
        {
        public:
            explicit DataBlockMessageType(FirmwareUpgradeCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryServer& parent;
        };

        class VerifyMessageType
            : public CanMessageType
        {
        public:
            explicit VerifyMessageType(FirmwareUpgradeCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryServer& parent;
        };

        class ActivateMessageType
            : public CanMessageType
        {
        public:
            explicit ActivateMessageType(FirmwareUpgradeCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryServer& parent;
        };

        class AbortMessageType
            : public CanMessageType
        {
        public:
            explicit AbortMessageType(FirmwareUpgradeCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryServer& parent;
        };

        class QueryProgressMessageType
            : public CanMessageType
        {
        public:
            explicit QueryProgressMessageType(FirmwareUpgradeCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryServer& parent;
        };

        void ResetSessionTimer();
        void StopSessionTimer();
        void HandleSessionTimeout();

        CanFrameTransport& transport;
        Config config;
        infra::TimerSingleShot sessionTimeoutTimer;

        BeginUpgradeMessageType beginUpgrade;
        DataBlockMessageType dataBlock;
        VerifyMessageType verify;
        ActivateMessageType activate;
        AbortMessageType abort;
        QueryProgressMessageType queryProgress;
    };
}
