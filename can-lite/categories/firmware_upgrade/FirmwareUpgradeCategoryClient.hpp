#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
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
        FirmwareUpgradeCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource);

        uint8_t Id() const override;

        bool SendBeginUpgrade(uint16_t targetNodeId, uint32_t firmwareSize);
        bool SendDataBlock(uint16_t targetNodeId, uint16_t blockIndex, const hal::Can::Message& blockData);
        bool SendVerify(uint16_t targetNodeId, uint32_t expectedCrc32);
        bool SendActivate(uint16_t targetNodeId);
        bool SendAbort(uint16_t targetNodeId);
        bool SendQueryProgress(uint16_t targetNodeId);

    private:
        void HandleBeginResponse(const hal::Can::Message& data);
        void HandleDataBlockAck(const hal::Can::Message& data);
        void HandleVerifyResponse(const hal::Can::Message& data);
        void HandleActivateResponse(const hal::Can::Message& data);
        void HandleProgressResponse(const hal::Can::Message& data);

        CanMessageHandler<FirmwareUpgradeCategoryClient> beginResponse{ fwuBeginResponseId, *this, &FirmwareUpgradeCategoryClient::HandleBeginResponse };
        CanMessageHandler<FirmwareUpgradeCategoryClient> dataBlockAck{ fwuDataBlockAckId, *this, &FirmwareUpgradeCategoryClient::HandleDataBlockAck };
        CanMessageHandler<FirmwareUpgradeCategoryClient> verifyResponse{ fwuVerifyResponseId, *this, &FirmwareUpgradeCategoryClient::HandleVerifyResponse };
        CanMessageHandler<FirmwareUpgradeCategoryClient> activateResponse{ fwuActivateResponseId, *this, &FirmwareUpgradeCategoryClient::HandleActivateResponse };
        CanMessageHandler<FirmwareUpgradeCategoryClient> progressResponse{ fwuProgressResponseId, *this, &FirmwareUpgradeCategoryClient::HandleProgressResponse };
    };
}
