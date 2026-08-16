#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"

namespace services
{
    FirmwareUpgradeCategoryClient::FirmwareUpgradeCategoryClient()
        : CanCategoryClient(messageTypeStorage)
    {
        AddMessageType(fwuBeginResponseId, [this](infra::ConstByteRange payload)
            {
                return HandleBeginResponse(payload);
            });
        AddMessageType(fwuDataBlockAckId, [this](infra::ConstByteRange payload)
            {
                return HandleDataBlockAck(payload);
            });
        AddMessageType(fwuVerifyResponseId, [this](infra::ConstByteRange payload)
            {
                return HandleVerifyResponse(payload);
            });
        AddMessageType(fwuActivateResponseId, [this](infra::ConstByteRange payload)
            {
                return HandleActivateResponse(payload);
            });
        AddMessageType(fwuProgressResponseId, [this](infra::ConstByteRange payload)
            {
                return HandleProgressResponse(payload);
            });
    }

    uint8_t FirmwareUpgradeCategoryClient::Id() const
    {
        return firmwareUpgradeCategoryId;
    }

    bool FirmwareUpgradeCategoryClient::RequiresSequenceValidation() const
    {
        return false;
    }

    bool FirmwareUpgradeCategoryClient::SendBeginUpgrade(uint16_t targetNodeId, uint32_t firmwareSize)
    {
        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteUInt32(data, 0, firmwareSize);
        return Outbound().SendTo(targetNodeId, CanPriority::command, fwuBeginUpgradeId, data);
    }

    bool FirmwareUpgradeCategoryClient::SendDataBlock(uint16_t targetNodeId, uint16_t blockIndex, const hal::Can::Message& blockData)
    {
        if (blockData.size() > 6)
            return false;

        hal::Can::Message data;
        data.resize(2, 0);
        CanFrameCodec::WriteUInt16(data, 0, blockIndex);
        for (auto byte : blockData)
            data.push_back(byte);
        return Outbound().SendTo(targetNodeId, CanPriority::command, fwuDataBlockId, data);
    }

    bool FirmwareUpgradeCategoryClient::SendVerify(uint16_t targetNodeId, uint32_t expectedCrc32)
    {
        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteUInt32(data, 0, expectedCrc32);
        return Outbound().SendTo(targetNodeId, CanPriority::command, fwuVerifyId, data);
    }

    bool FirmwareUpgradeCategoryClient::SendActivate(uint16_t targetNodeId)
    {
        hal::Can::Message data;
        return Outbound().SendTo(targetNodeId, CanPriority::command, fwuActivateId, data);
    }

    bool FirmwareUpgradeCategoryClient::SendAbort(uint16_t targetNodeId)
    {
        hal::Can::Message data;
        return Outbound().SendTo(targetNodeId, CanPriority::command, fwuAbortId, data);
    }

    bool FirmwareUpgradeCategoryClient::SendQueryProgress(uint16_t targetNodeId)
    {
        hal::Can::Message data;
        return Outbound().SendTo(targetNodeId, CanPriority::command, fwuQueryProgressId, data);
    }

    bool FirmwareUpgradeCategoryClient::HandleBeginResponse(infra::ConstByteRange payload)
    {
        if (payload.size() < 3)
            return false;

        auto status = static_cast<FwuError>(payload[0]);
        auto pageSize = CanFrameCodec::ReadUInt16(payload, 1);

        NotifyObservers([status, pageSize](auto& observer)
            {
                observer.OnBeginResponse(status, pageSize);
            });

        return true;
    }

    bool FirmwareUpgradeCategoryClient::HandleDataBlockAck(infra::ConstByteRange payload)
    {
        if (payload.size() < 3)
            return false;

        auto status = static_cast<FwuError>(payload[0]);
        auto blockIndex = CanFrameCodec::ReadUInt16(payload, 1);

        NotifyObservers([status, blockIndex](auto& observer)
            {
                observer.OnDataBlockAck(status, blockIndex);
            });

        return true;
    }

    bool FirmwareUpgradeCategoryClient::HandleVerifyResponse(infra::ConstByteRange payload)
    {
        if (payload.empty())
            return false;

        auto status = static_cast<FwuError>(payload.front());

        NotifyObservers([status](auto& observer)
            {
                observer.OnVerifyResponse(status);
            });

        return true;
    }

    bool FirmwareUpgradeCategoryClient::HandleActivateResponse(infra::ConstByteRange payload)
    {
        if (payload.empty())
            return false;

        auto status = static_cast<FwuError>(payload.front());

        NotifyObservers([status](auto& observer)
            {
                observer.OnActivateResponse(status);
            });

        return true;
    }

    bool FirmwareUpgradeCategoryClient::HandleProgressResponse(infra::ConstByteRange payload)
    {
        if (payload.size() < 5)
            return false;

        auto state = static_cast<FwuState>(payload[0]);
        auto blocksReceived = CanFrameCodec::ReadUInt16(payload, 1);
        auto totalBlocks = CanFrameCodec::ReadUInt16(payload, 3);

        NotifyObservers([state, blocksReceived, totalBlocks](auto& observer)
            {
                observer.OnProgressResponse(state, blocksReceived, totalBlocks);
            });

        return true;
    }
}
