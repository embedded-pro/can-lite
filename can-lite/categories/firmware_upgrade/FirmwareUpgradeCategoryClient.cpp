#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"

namespace services
{
    FirmwareUpgradeCategoryClient::FirmwareUpgradeCategoryClient(CanFrameTransport& transport, CanProtocolClient& client)
        : transport(transport)
        , client(client)
        , beginResponse(*this)
        , dataBlockAck(*this)
        , verifyResponse(*this)
        , activateResponse(*this)
        , progressResponse(*this)
    {
        AddMessageType(beginResponse);
        AddMessageType(dataBlockAck);
        AddMessageType(verifyResponse);
        AddMessageType(activateResponse);
        AddMessageType(progressResponse);
    }

    uint8_t FirmwareUpgradeCategoryClient::Id() const
    {
        return firmwareUpgradeCategoryId;
    }

    bool FirmwareUpgradeCategoryClient::SendBeginUpgrade(uint16_t targetNodeId, uint32_t firmwareSize)
    {
        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteUInt32(data, 0, firmwareSize);
        return transport.SendFrame(targetNodeId, CanPriority::command, firmwareUpgradeCategoryId, fwuBeginUpgradeId, data, [] {});
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
        return transport.SendFrame(targetNodeId, CanPriority::command, firmwareUpgradeCategoryId, fwuDataBlockId, data, [] {});
    }

    bool FirmwareUpgradeCategoryClient::SendVerify(uint16_t targetNodeId, uint32_t expectedCrc32)
    {
        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteUInt32(data, 0, expectedCrc32);
        return transport.SendFrame(targetNodeId, CanPriority::command, firmwareUpgradeCategoryId, fwuVerifyId, data, [] {});
    }

    bool FirmwareUpgradeCategoryClient::SendActivate(uint16_t targetNodeId)
    {
        hal::Can::Message data;
        return transport.SendFrame(targetNodeId, CanPriority::command, firmwareUpgradeCategoryId, fwuActivateId, data, [] {});
    }

    bool FirmwareUpgradeCategoryClient::SendAbort(uint16_t targetNodeId)
    {
        hal::Can::Message data;
        return transport.SendFrame(targetNodeId, CanPriority::command, firmwareUpgradeCategoryId, fwuAbortId, data, [] {});
    }

    bool FirmwareUpgradeCategoryClient::SendQueryProgress(uint16_t targetNodeId)
    {
        hal::Can::Message data;
        return transport.SendFrame(targetNodeId, CanPriority::command, firmwareUpgradeCategoryId, fwuQueryProgressId, data, [] {});
    }

    // BeginResponse

    FirmwareUpgradeCategoryClient::BeginResponseMessageType::BeginResponseMessageType(FirmwareUpgradeCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryClient::BeginResponseMessageType::Id() const
    {
        return fwuBeginResponseId;
    }

    void FirmwareUpgradeCategoryClient::BeginResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 3)
            return;

        auto status = static_cast<FwuError>(data[0]);
        auto pageSize = CanFrameCodec::ReadUInt16(data, 1);

        parent.NotifyObservers([status, pageSize](auto& observer)
            {
                observer.OnBeginResponse(status, pageSize);
            });
    }

    // DataBlockAck

    FirmwareUpgradeCategoryClient::DataBlockAckMessageType::DataBlockAckMessageType(FirmwareUpgradeCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryClient::DataBlockAckMessageType::Id() const
    {
        return fwuDataBlockAckId;
    }

    void FirmwareUpgradeCategoryClient::DataBlockAckMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 3)
            return;

        auto status = static_cast<FwuError>(data[0]);
        auto blockIndex = CanFrameCodec::ReadUInt16(data, 1);

        parent.NotifyObservers([status, blockIndex](auto& observer)
            {
                observer.OnDataBlockAck(status, blockIndex);
            });
    }

    // VerifyResponse

    FirmwareUpgradeCategoryClient::VerifyResponseMessageType::VerifyResponseMessageType(FirmwareUpgradeCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryClient::VerifyResponseMessageType::Id() const
    {
        return fwuVerifyResponseId;
    }

    void FirmwareUpgradeCategoryClient::VerifyResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.empty())
            return;

        auto status = static_cast<FwuError>(data[0]);

        parent.NotifyObservers([status](auto& observer)
            {
                observer.OnVerifyResponse(status);
            });
    }

    // ActivateResponse

    FirmwareUpgradeCategoryClient::ActivateResponseMessageType::ActivateResponseMessageType(FirmwareUpgradeCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryClient::ActivateResponseMessageType::Id() const
    {
        return fwuActivateResponseId;
    }

    void FirmwareUpgradeCategoryClient::ActivateResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.empty())
            return;

        auto status = static_cast<FwuError>(data[0]);

        parent.NotifyObservers([status](auto& observer)
            {
                observer.OnActivateResponse(status);
            });
    }

    // ProgressResponse

    FirmwareUpgradeCategoryClient::ProgressResponseMessageType::ProgressResponseMessageType(FirmwareUpgradeCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryClient::ProgressResponseMessageType::Id() const
    {
        return fwuProgressResponseId;
    }

    void FirmwareUpgradeCategoryClient::ProgressResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 5)
            return;

        auto state = static_cast<FwuState>(data[0]);
        auto blocksReceived = CanFrameCodec::ReadUInt16(data, 1);
        auto totalBlocks = CanFrameCodec::ReadUInt16(data, 3);

        parent.NotifyObservers([state, blocksReceived, totalBlocks](auto& observer)
            {
                observer.OnProgressResponse(state, blocksReceived, totalBlocks);
            });
    }
}
