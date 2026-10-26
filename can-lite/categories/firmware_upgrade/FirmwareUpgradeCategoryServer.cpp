#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace services
{
    namespace
    {
        CanAckStatus ToAckStatus(FwuError status)
        {
            return status == FwuError::ok ? CanAckStatus::success : CanAckStatus::categoryError;
        }
    }

    FirmwareUpgradeCategoryServer::FirmwareUpgradeCategoryServer(CanFrameTransport& transport)
        : FirmwareUpgradeCategoryServer(transport, Config{})
    {}

    FirmwareUpgradeCategoryServer::FirmwareUpgradeCategoryServer(CanFrameTransport& transport, const Config& config)
        : CanCategoryServer(transport)
        , config(config)
    {
        AddMessageTypes(beginUpgrade, dataBlock, verify, activate, abortUpgrade, queryProgress);
    }

    uint8_t FirmwareUpgradeCategoryServer::Id() const
    {
        return firmwareUpgradeCategoryId;
    }

    bool FirmwareUpgradeCategoryServer::RequiresSequenceValidation() const
    {
        return false;
    }

    void FirmwareUpgradeCategoryServer::CompleteBeginUpgrade(FwuError status, uint16_t pageSize)
    {
        if (status == FwuError::ok)
            StartSessionTimer();

        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(status)).WriteUInt16(pageSize);

        SendResponse(fwuBeginResponseId, payload);
        SendCommandAck(fwuBeginUpgradeId, ToAckStatus(status));
    }

    void FirmwareUpgradeCategoryServer::CompleteDataBlock(uint16_t blockIndex, FwuError status)
    {
        if (status == FwuError::ok)
            StartSessionTimer();

        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(status)).WriteUInt16(blockIndex);

        SendResponse(fwuDataBlockAckId, payload);
        SendCommandAck(fwuDataBlockId, ToAckStatus(status));
    }

    void FirmwareUpgradeCategoryServer::CompleteVerify(FwuError status)
    {
        CompleteWithStatus(fwuVerifyId, fwuVerifyResponseId, status);
    }

    void FirmwareUpgradeCategoryServer::CompleteActivate(FwuError status)
    {
        CompleteWithStatus(fwuActivateId, fwuActivateResponseId, status);
    }

    void FirmwareUpgradeCategoryServer::CompleteAbort()
    {
        SendCommandAck(fwuAbortId, CanAckStatus::success);
    }

    void FirmwareUpgradeCategoryServer::CompleteQueryProgress(FwuState state, uint16_t blocksReceived, uint16_t totalBlocks)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(state)).WriteUInt16(blocksReceived).WriteUInt16(totalBlocks);

        SendResponse(fwuProgressResponseId, payload);
        SendCommandAck(fwuQueryProgressId, CanAckStatus::success);
    }

    void FirmwareUpgradeCategoryServer::CompleteWithStatus(uint8_t commandId, uint8_t responseId, FwuError status)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(status));

        SendResponse(responseId, payload);
        SendCommandAck(commandId, ToAckStatus(status));
    }

    void FirmwareUpgradeCategoryServer::StartSessionTimer()
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

    bool FirmwareUpgradeCategoryServer::HandleBeginUpgrade(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto firmwareSize = reader.ReadUInt32();

        if (!reader.Valid())
            return false;

        if (!HasObserver())
            CompleteBeginUpgrade(FwuError::notReady, 0);
        else
            GetObserver().OnBeginUpgrade(firmwareSize, [this](FwuError status, uint16_t pageSize)
                {
                    CompleteBeginUpgrade(status, pageSize);
                });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleDataBlock(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto blockIndex = reader.ReadUInt16();
        auto blockData = reader.ReadRemaining();

        if (!reader.Valid())
            return false;

        if (!HasObserver())
            CompleteDataBlock(blockIndex, FwuError::notReady);
        else
            GetObserver().OnDataBlock(blockIndex, blockData, [this, blockIndex](FwuError status)
                {
                    CompleteDataBlock(blockIndex, status);
                });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleVerify(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto expectedCrc32 = reader.ReadUInt32();

        if (!reader.Valid())
            return false;

        StopSessionTimer();

        if (!HasObserver())
            CompleteVerify(FwuError::notReady);
        else
            GetObserver().OnVerify(expectedCrc32, [this](FwuError status)
                {
                    CompleteVerify(status);
                });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleActivate(const hal::Can::Message&)
    {
        StopSessionTimer();

        if (!HasObserver())
            CompleteActivate(FwuError::notReady);
        else
            GetObserver().OnActivate([this](FwuError status)
                {
                    CompleteActivate(status);
                });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleAbort(const hal::Can::Message&)
    {
        StopSessionTimer();

        if (!HasObserver())
            CompleteAbort();
        else
            GetObserver().OnAbort([this]()
                {
                    CompleteAbort();
                });

        return true;
    }

    bool FirmwareUpgradeCategoryServer::HandleQueryProgress(const hal::Can::Message&)
    {
        if (!HasObserver())
            CompleteQueryProgress(FwuState::idle, 0, 0);
        else
            GetObserver().OnQueryProgress([this](FwuState state, uint16_t blocksReceived, uint16_t totalBlocks)
                {
                    CompleteQueryProgress(state, blocksReceived, totalBlocks);
                });

        return true;
    }
}
