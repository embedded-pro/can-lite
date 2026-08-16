#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/core/CanFrameCodec.hpp"

namespace services
{
    FirmwareUpgradeCategoryServer::FirmwareUpgradeCategoryServer(const Config& config)
        : CanCategoryServer(messageTypeStorage)
        , config(config)
    {
        AddMessageType(fwuBeginUpgradeId, [this](infra::ConstByteRange payload)
            {
                return HandleBeginUpgrade(payload);
            });
        AddMessageType(fwuDataBlockId, [this](infra::ConstByteRange payload)
            {
                return HandleDataBlock(payload);
            });
        AddMessageType(fwuVerifyId, [this](infra::ConstByteRange payload)
            {
                return HandleVerify(payload);
            });
        AddMessageType(fwuActivateId, [this](infra::ConstByteRange payload)
            {
                return HandleActivate(payload);
            });
        AddMessageType(fwuAbortId, [this](infra::ConstByteRange payload)
            {
                return HandleAbort(payload);
            });
        AddMessageType(fwuQueryProgressId, [this](infra::ConstByteRange payload)
            {
                return HandleQueryProgress(payload);
            });
    }

    uint8_t FirmwareUpgradeCategoryServer::Id() const
    {
        return firmwareUpgradeCategoryId;
    }

    bool FirmwareUpgradeCategoryServer::RequiresSequenceValidation() const
    {
        return false;
    }

    bool FirmwareUpgradeCategoryServer::HandleBeginUpgrade(infra::ConstByteRange payload)
    {
        if (payload.size() < 4)
            return false;

        auto firmwareSize = CanFrameCodec::ReadUInt32(payload, 0);
        ResetSessionTimer();

        NotifyObservers([this, firmwareSize](auto& observer)
            {
                observer.OnBeginUpgrade(firmwareSize, [this](FwuError status, uint16_t pageSize)
                    {
                        SendBeginResponse(status, pageSize);
                        SendCommandAck(fwuBeginUpgradeId, CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleDataBlock(infra::ConstByteRange payload)
    {
        if (payload.size() < 2)
            return false;

        auto blockIndex = CanFrameCodec::ReadUInt16(payload, 0);
        ResetSessionTimer();

        auto blockData = infra::DiscardHead(payload, 2);

        NotifyObservers([this, blockIndex, blockData](auto& observer)
            {
                observer.OnDataBlock(blockIndex, blockData, [this, blockIndex](FwuError status)
                    {
                        SendDataBlockAck(status, blockIndex);
                        SendCommandAck(fwuDataBlockId, CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleVerify(infra::ConstByteRange payload)
    {
        if (payload.size() < 4)
            return false;

        auto expectedCrc32 = CanFrameCodec::ReadUInt32(payload, 0);
        StopSessionTimer();

        NotifyObservers([this, expectedCrc32](auto& observer)
            {
                observer.OnVerify(expectedCrc32, [this](FwuError status)
                    {
                        SendVerifyResponse(status);
                        SendCommandAck(fwuVerifyId, CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleActivate(infra::ConstByteRange)
    {
        StopSessionTimer();

        NotifyObservers([this](auto& observer)
            {
                observer.OnActivate([this](FwuError status)
                    {
                        SendActivateResponse(status);
                        SendCommandAck(fwuActivateId, CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleAbort(infra::ConstByteRange)
    {
        StopSessionTimer();

        NotifyObservers([this](auto& observer)
            {
                observer.OnAbort([this]()
                    {
                        SendCommandAck(fwuAbortId, CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleQueryProgress(infra::ConstByteRange)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnQueryProgress([this](FwuState state, uint16_t blocksReceived, uint16_t totalBlocks)
                    {
                        SendProgressResponse(state, blocksReceived, totalBlocks);
                        SendCommandAck(fwuQueryProgressId, CanAckStatus::success);
                    });
            });

        return true;
    }

    void FirmwareUpgradeCategoryServer::SendBeginResponse(FwuError status, uint16_t pageSize)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = static_cast<uint8_t>(status);
        CanFrameCodec::WriteUInt16(data, 1, pageSize);
        Outbound().Send(CanPriority::response, fwuBeginResponseId, data);
    }

    void FirmwareUpgradeCategoryServer::SendDataBlockAck(FwuError status, uint16_t blockIndex)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = static_cast<uint8_t>(status);
        CanFrameCodec::WriteUInt16(data, 1, blockIndex);
        Outbound().Send(CanPriority::response, fwuDataBlockAckId, data);
    }

    void FirmwareUpgradeCategoryServer::SendVerifyResponse(FwuError status)
    {
        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(status));
        Outbound().Send(CanPriority::response, fwuVerifyResponseId, data);
    }

    void FirmwareUpgradeCategoryServer::SendActivateResponse(FwuError status)
    {
        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(status));
        Outbound().Send(CanPriority::response, fwuActivateResponseId, data);
    }

    void FirmwareUpgradeCategoryServer::SendProgressResponse(FwuState state, uint16_t blocksReceived, uint16_t totalBlocks)
    {
        hal::Can::Message data;
        data.resize(5, 0);
        data[0] = static_cast<uint8_t>(state);
        CanFrameCodec::WriteUInt16(data, 1, blocksReceived);
        CanFrameCodec::WriteUInt16(data, 3, totalBlocks);
        Outbound().Send(CanPriority::response, fwuProgressResponseId, data);
    }

    void FirmwareUpgradeCategoryServer::ResetSessionTimer()
    {
        sessionTimeoutTimer.Start(config.sessionTimeout, [this]()
            {
                HandleSessionTimeout();
            });
    }

    void FirmwareUpgradeCategoryServer::StopSessionTimer()
    {
        sessionTimeoutTimer.Cancel();
    }

    void FirmwareUpgradeCategoryServer::HandleSessionTimeout()
    {
        NotifyObservers([](auto& observer)
            {
                observer.OnSessionTimeout();
            });
    }
}
