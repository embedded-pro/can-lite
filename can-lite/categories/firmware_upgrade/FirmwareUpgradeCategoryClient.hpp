#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageType.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class CanProtocolClient;

    class FirmwareUpgradeCategoryClient;

    class FirmwareUpgradeCategoryClientObserver
        : public infra::SingleObserver<FirmwareUpgradeCategoryClientObserver, FirmwareUpgradeCategoryClient>
    {
    public:
        using infra::SingleObserver<FirmwareUpgradeCategoryClientObserver, FirmwareUpgradeCategoryClient>::SingleObserver;

        virtual void OnBeginResponse(FwuError status, uint16_t pageSize) = 0;
        virtual void OnDataBlockAck(FwuError status, uint16_t blockIndex) = 0;
        virtual void OnVerifyResponse(FwuError status) = 0;
        virtual void OnActivateResponse(FwuError status) = 0;
        virtual void OnProgressResponse(FwuState state, uint16_t blocksReceived, uint16_t totalBlocks) = 0;
    };

    class FirmwareUpgradeCategoryClient
        : public CanCategoryClient
        , public infra::Subject<FirmwareUpgradeCategoryClientObserver>
    {
    public:
        FirmwareUpgradeCategoryClient(CanFrameTransport& transport, CanProtocolClient& client);

        uint8_t Id() const override;

        bool SendBeginUpgrade(uint16_t targetNodeId, uint32_t firmwareSize);
        bool SendDataBlock(uint16_t targetNodeId, uint16_t blockIndex, const hal::Can::Message& blockData);
        bool SendVerify(uint16_t targetNodeId, uint32_t expectedCrc32);
        bool SendActivate(uint16_t targetNodeId);
        bool SendAbort(uint16_t targetNodeId);
        bool SendQueryProgress(uint16_t targetNodeId);

    private:
        class BeginResponseMessageType
            : public CanMessageType
        {
        public:
            explicit BeginResponseMessageType(FirmwareUpgradeCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryClient& parent;
        };

        class DataBlockAckMessageType
            : public CanMessageType
        {
        public:
            explicit DataBlockAckMessageType(FirmwareUpgradeCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryClient& parent;
        };

        class VerifyResponseMessageType
            : public CanMessageType
        {
        public:
            explicit VerifyResponseMessageType(FirmwareUpgradeCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryClient& parent;
        };

        class ActivateResponseMessageType
            : public CanMessageType
        {
        public:
            explicit ActivateResponseMessageType(FirmwareUpgradeCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryClient& parent;
        };

        class ProgressResponseMessageType
            : public CanMessageType
        {
        public:
            explicit ProgressResponseMessageType(FirmwareUpgradeCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FirmwareUpgradeCategoryClient& parent;
        };

        CanFrameTransport& transport;
        CanProtocolClient& client;

        BeginResponseMessageType beginResponse;
        DataBlockAckMessageType dataBlockAck;
        VerifyResponseMessageType verifyResponse;
        ActivateResponseMessageType activateResponse;
        ProgressResponseMessageType progressResponse;
    };
}
