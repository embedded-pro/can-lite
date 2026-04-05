#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/core/CanFrameCodec.hpp"

namespace services
{
    FirmwareUpgradeCategoryServer::FirmwareUpgradeCategoryServer(CanFrameTransport& transport, const Config& config)
        : transport(transport)
        , config(config)
        , beginUpgrade(*this)
        , dataBlock(*this)
        , verify(*this)
        , activate(*this)
        , abort(*this)
        , queryProgress(*this)
    {
        AddMessageType(beginUpgrade);
        AddMessageType(dataBlock);
        AddMessageType(verify);
        AddMessageType(activate);
        AddMessageType(abort);
        AddMessageType(queryProgress);
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
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = static_cast<uint8_t>(status);
        CanFrameCodec::WriteInt16(data, 1, static_cast<int16_t>(pageSize));
        transport.SendFrame(CanPriority::response, firmwareUpgradeCategoryId, fwuBeginResponseId, data, [] {});
    }

    void FirmwareUpgradeCategoryServer::SendDataBlockAck(FwuError status, uint16_t blockIndex)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = static_cast<uint8_t>(status);
        CanFrameCodec::WriteInt16(data, 1, static_cast<int16_t>(blockIndex));
        transport.SendFrame(CanPriority::response, firmwareUpgradeCategoryId, fwuDataBlockAckId, data, [] {});
    }

    void FirmwareUpgradeCategoryServer::SendVerifyResponse(FwuError status)
    {
        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(status));
        transport.SendFrame(CanPriority::response, firmwareUpgradeCategoryId, fwuVerifyResponseId, data, [] {});
    }

    void FirmwareUpgradeCategoryServer::SendActivateResponse(FwuError status)
    {
        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(status));
        transport.SendFrame(CanPriority::response, firmwareUpgradeCategoryId, fwuActivateResponseId, data, [] {});
    }

    void FirmwareUpgradeCategoryServer::SendProgressResponse(FwuState state, uint16_t blocksReceived, uint16_t totalBlocks)
    {
        hal::Can::Message data;
        data.resize(5, 0);
        data[0] = static_cast<uint8_t>(state);
        CanFrameCodec::WriteInt16(data, 1, static_cast<int16_t>(blocksReceived));
        CanFrameCodec::WriteInt16(data, 3, static_cast<int16_t>(totalBlocks));
        transport.SendFrame(CanPriority::response, firmwareUpgradeCategoryId, fwuProgressResponseId, data, [] {});
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

    // BeginUpgrade

    FirmwareUpgradeCategoryServer::BeginUpgradeMessageType::BeginUpgradeMessageType(FirmwareUpgradeCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryServer::BeginUpgradeMessageType::Id() const
    {
        return fwuBeginUpgradeId;
    }

    void FirmwareUpgradeCategoryServer::BeginUpgradeMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 4)
            return;

        auto firmwareSize = static_cast<uint32_t>(CanFrameCodec::ReadInt32(data, 0));
        parent.ResetSessionTimer();

        parent.NotifyObservers([firmwareSize](auto& observer)
            {
                observer.OnBeginUpgrade(firmwareSize);
            });
    }

    // DataBlock

    FirmwareUpgradeCategoryServer::DataBlockMessageType::DataBlockMessageType(FirmwareUpgradeCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryServer::DataBlockMessageType::Id() const
    {
        return fwuDataBlockId;
    }

    void FirmwareUpgradeCategoryServer::DataBlockMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 2)
            return;

        auto blockIndex = static_cast<uint16_t>(CanFrameCodec::ReadInt16(data, 0));
        parent.ResetSessionTimer();

        parent.NotifyObservers([blockIndex, &data](auto& observer)
            {
                observer.OnDataBlock(blockIndex, data);
            });
    }

    // Verify

    FirmwareUpgradeCategoryServer::VerifyMessageType::VerifyMessageType(FirmwareUpgradeCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryServer::VerifyMessageType::Id() const
    {
        return fwuVerifyId;
    }

    void FirmwareUpgradeCategoryServer::VerifyMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 4)
            return;

        auto expectedCrc32 = static_cast<uint32_t>(CanFrameCodec::ReadInt32(data, 0));
        parent.StopSessionTimer();

        parent.NotifyObservers([expectedCrc32](auto& observer)
            {
                observer.OnVerify(expectedCrc32);
            });
    }

    // Activate

    FirmwareUpgradeCategoryServer::ActivateMessageType::ActivateMessageType(FirmwareUpgradeCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryServer::ActivateMessageType::Id() const
    {
        return fwuActivateId;
    }

    void FirmwareUpgradeCategoryServer::ActivateMessageType::Handle(const hal::Can::Message& data)
    {
        parent.StopSessionTimer();

        parent.NotifyObservers([](auto& observer)
            {
                observer.OnActivate();
            });
    }

    // Abort

    FirmwareUpgradeCategoryServer::AbortMessageType::AbortMessageType(FirmwareUpgradeCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryServer::AbortMessageType::Id() const
    {
        return fwuAbortId;
    }

    void FirmwareUpgradeCategoryServer::AbortMessageType::Handle(const hal::Can::Message& data)
    {
        parent.StopSessionTimer();

        parent.NotifyObservers([](auto& observer)
            {
                observer.OnAbort();
            });
    }

    // QueryProgress

    FirmwareUpgradeCategoryServer::QueryProgressMessageType::QueryProgressMessageType(FirmwareUpgradeCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FirmwareUpgradeCategoryServer::QueryProgressMessageType::Id() const
    {
        return fwuQueryProgressId;
    }

    void FirmwareUpgradeCategoryServer::QueryProgressMessageType::Handle(const hal::Can::Message& data)
    {
        parent.NotifyObservers([](auto& observer)
            {
                observer.OnQueryProgress();
            });
    }
}
