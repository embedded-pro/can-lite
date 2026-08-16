#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
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
        : private CanCategoryHandlerStorage<5>
        , public CanCategoryClient
        , public infra::Subject<FirmwareUpgradeCategoryClientObserver>
    {
    public:
        FirmwareUpgradeCategoryClient();

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

        bool SendBeginUpgrade(uint16_t targetNodeId, uint32_t firmwareSize);
        bool SendDataBlock(uint16_t targetNodeId, uint16_t blockIndex, const hal::Can::Message& blockData);
        bool SendVerify(uint16_t targetNodeId, uint32_t expectedCrc32);
        bool SendActivate(uint16_t targetNodeId);
        bool SendAbort(uint16_t targetNodeId);
        bool SendQueryProgress(uint16_t targetNodeId);

    private:
        bool HandleBeginResponse(infra::ConstByteRange payload);
        bool HandleDataBlockAck(infra::ConstByteRange payload);
        bool HandleVerifyResponse(infra::ConstByteRange payload);
        bool HandleActivateResponse(infra::ConstByteRange payload);
        bool HandleProgressResponse(infra::ConstByteRange payload);
    };
}
