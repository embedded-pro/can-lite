#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace services
{
    FirmwareUpgradeCategoryClient::FirmwareUpgradeCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource)
        : CanCategoryClient(transport, sequenceSource)
    {
        AddMessageTypes(beginResponse, dataBlockAck, verifyResponse, activateResponse, progressResponse);
    }

    uint8_t FirmwareUpgradeCategoryClient::Id() const
    {
        return firmwareUpgradeCategoryId;
    }

    // Commands carry no sequence byte: the server orders transfers by block index instead.

    bool FirmwareUpgradeCategoryClient::SendBeginUpgrade(uint16_t targetNodeId, uint32_t firmwareSize)
    {
        CanPayloadWriter payload;
        payload.WriteUInt32(firmwareSize);

        return SendCommandWithoutSequence(targetNodeId, fwuBeginUpgradeId, payload);
    }

    bool FirmwareUpgradeCategoryClient::SendDataBlock(uint16_t targetNodeId, uint16_t blockIndex, const hal::Can::Message& blockData)
    {
        CanPayloadWriter payload;
        payload.WriteUInt16(blockIndex).WriteBytes(infra::MakeRange(blockData));

        return SendCommandWithoutSequence(targetNodeId, fwuDataBlockId, payload);
    }

    bool FirmwareUpgradeCategoryClient::SendVerify(uint16_t targetNodeId, uint32_t expectedCrc32)
    {
        CanPayloadWriter payload;
        payload.WriteUInt32(expectedCrc32);

        return SendCommandWithoutSequence(targetNodeId, fwuVerifyId, payload);
    }

    bool FirmwareUpgradeCategoryClient::SendActivate(uint16_t targetNodeId)
    {
        return SendCommandWithoutSequence(targetNodeId, fwuActivateId);
    }

    bool FirmwareUpgradeCategoryClient::SendAbort(uint16_t targetNodeId)
    {
        return SendCommandWithoutSequence(targetNodeId, fwuAbortId);
    }

    bool FirmwareUpgradeCategoryClient::SendQueryProgress(uint16_t targetNodeId)
    {
        return SendCommandWithoutSequence(targetNodeId, fwuQueryProgressId);
    }

    void FirmwareUpgradeCategoryClient::HandleBeginResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto status = static_cast<FwuError>(reader.ReadUInt8());
        auto pageSize = reader.ReadUInt16();

        if (!reader.Valid())
            return;

        NotifyObservers([status, pageSize](auto& observer)
            {
                observer.OnBeginResponse(status, pageSize);
            });
    }

    void FirmwareUpgradeCategoryClient::HandleDataBlockAck(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto status = static_cast<FwuError>(reader.ReadUInt8());
        auto blockIndex = reader.ReadUInt16();

        if (!reader.Valid())
            return;

        NotifyObservers([status, blockIndex](auto& observer)
            {
                observer.OnDataBlockAck(status, blockIndex);
            });
    }

    void FirmwareUpgradeCategoryClient::HandleVerifyResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto status = static_cast<FwuError>(reader.ReadUInt8());

        if (!reader.Valid())
            return;

        NotifyObservers([status](auto& observer)
            {
                observer.OnVerifyResponse(status);
            });
    }

    void FirmwareUpgradeCategoryClient::HandleActivateResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto status = static_cast<FwuError>(reader.ReadUInt8());

        if (!reader.Valid())
            return;

        NotifyObservers([status](auto& observer)
            {
                observer.OnActivateResponse(status);
            });
    }

    void FirmwareUpgradeCategoryClient::HandleProgressResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto state = static_cast<FwuState>(reader.ReadUInt8());
        auto blocksReceived = reader.ReadUInt16();
        auto totalBlocks = reader.ReadUInt16();

        if (!reader.Valid())
            return;

        NotifyObservers([state, blocksReceived, totalBlocks](auto& observer)
            {
                observer.OnProgressResponse(state, blocksReceived, totalBlocks);
            });
    }
}
