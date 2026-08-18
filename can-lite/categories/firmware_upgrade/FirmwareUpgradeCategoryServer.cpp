#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace services
{
    FirmwareUpgradeCategoryServer::FirmwareUpgradeCategoryServer(CanFrameTransport& transport, const Config& config)
        : CanCategoryServer(transport)
        , config(config)
    {
        AddMessageTypes(beginUpgrade, dataBlock, verify, activate, abort, queryProgress);
    }

    uint8_t FirmwareUpgradeCategoryServer::Id() const
    {
        return firmwareUpgradeCategoryId;
    }

    bool FirmwareUpgradeCategoryServer::RequiresSequenceValidation() const
    {
        return false;
    }

    void FirmwareUpgradeCategoryServer::SendBeginResponse(FwuError status, uint16_t pageSize)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(status)).WriteUInt16(pageSize);

        SendResponse(fwuBeginResponseId, payload);
    }

    void FirmwareUpgradeCategoryServer::SendDataBlockAck(FwuError status, uint16_t blockIndex)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(status)).WriteUInt16(blockIndex);

        SendResponse(fwuDataBlockAckId, payload);
    }

    void FirmwareUpgradeCategoryServer::SendVerifyResponse(FwuError status)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(status));

        SendResponse(fwuVerifyResponseId, payload);
    }

    void FirmwareUpgradeCategoryServer::SendActivateResponse(FwuError status)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(status));

        SendResponse(fwuActivateResponseId, payload);
    }

    void FirmwareUpgradeCategoryServer::SendProgressResponse(FwuState state, uint16_t blocksReceived, uint16_t totalBlocks)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(state)).WriteUInt16(blocksReceived).WriteUInt16(totalBlocks);

        SendResponse(fwuProgressResponseId, payload);
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

    void FirmwareUpgradeCategoryServer::HandleBeginUpgrade(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto firmwareSize = reader.ReadUInt32();

        if (!reader.Valid())
        {
            SendCommandAck(fwuBeginUpgradeId, CanAckStatus::invalidPayload);
            return;
        }

        ResetSessionTimer();

        NotifyObservers([this, firmwareSize](auto& observer)
            {
                observer.OnBeginUpgrade(firmwareSize, [this](FwuError status, uint16_t pageSize)
                    {
                        SendBeginResponse(status, pageSize);
                        SendCommandAck(fwuBeginUpgradeId, status == FwuError::ok ? CanAckStatus::success : CanAckStatus::categoryError);
                    });
            });
    }

    void FirmwareUpgradeCategoryServer::HandleDataBlock(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto blockIndex = reader.ReadUInt16();

        if (!reader.Valid())
        {
            SendCommandAck(fwuDataBlockId, CanAckStatus::invalidPayload);
            return;
        }

        ResetSessionTimer();

        hal::Can::Message payload;
        for (auto byte : reader.ReadRemaining())
            payload.push_back(byte);

        NotifyObservers([this, blockIndex, payload](auto& observer)
            {
                observer.OnDataBlock(blockIndex, payload, [this, blockIndex](FwuError status)
                    {
                        SendDataBlockAck(status, blockIndex);
                        SendCommandAck(fwuDataBlockId, status == FwuError::ok ? CanAckStatus::success : CanAckStatus::categoryError);
                    });
            });
    }

    void FirmwareUpgradeCategoryServer::HandleVerify(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto expectedCrc32 = reader.ReadUInt32();

        if (!reader.Valid())
        {
            SendCommandAck(fwuVerifyId, CanAckStatus::invalidPayload);
            return;
        }

        StopSessionTimer();

        NotifyObservers([this, expectedCrc32](auto& observer)
            {
                observer.OnVerify(expectedCrc32, [this](FwuError status)
                    {
                        SendVerifyResponse(status);
                        SendCommandAck(fwuVerifyId, status == FwuError::ok ? CanAckStatus::success : CanAckStatus::categoryError);
                    });
            });
    }

    void FirmwareUpgradeCategoryServer::HandleActivate(const hal::Can::Message&)
    {
        StopSessionTimer();

        NotifyObservers([this](auto& observer)
            {
                observer.OnActivate([this](FwuError status)
                    {
                        SendActivateResponse(status);
                        SendCommandAck(fwuActivateId, status == FwuError::ok ? CanAckStatus::success : CanAckStatus::categoryError);
                    });
            });
    }

    void FirmwareUpgradeCategoryServer::HandleAbort(const hal::Can::Message&)
    {
        StopSessionTimer();

        NotifyObservers([this](auto& observer)
            {
                observer.OnAbort([this]()
                    {
                        SendCommandAck(fwuAbortId, CanAckStatus::success);
                    });
            });
    }

    void FirmwareUpgradeCategoryServer::HandleQueryProgress(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnQueryProgress([this](FwuState state, uint16_t blocksReceived, uint16_t totalBlocks)
                    {
                        SendProgressResponse(state, blocksReceived, totalBlocks);
                        SendCommandAck(fwuQueryProgressId, CanAckStatus::success);
                    });
            });
    }
}
