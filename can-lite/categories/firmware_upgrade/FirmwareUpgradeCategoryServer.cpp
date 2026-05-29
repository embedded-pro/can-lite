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
        CanFrameCodec::WriteUInt16(data, 1, pageSize);
        transport.SendFrame(CanPriority::response, firmwareUpgradeCategoryId, fwuBeginResponseId, data, [] {});
    }

    void FirmwareUpgradeCategoryServer::SendDataBlockAck(FwuError status, uint16_t blockIndex)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = static_cast<uint8_t>(status);
        CanFrameCodec::WriteUInt16(data, 1, blockIndex);
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
        CanFrameCodec::WriteUInt16(data, 1, blocksReceived);
        CanFrameCodec::WriteUInt16(data, 3, totalBlocks);
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
        {
            parent.SendCommandAck(fwuBeginUpgradeId, CanAckStatus::invalidPayload);
            return;
        }

        auto firmwareSize = CanFrameCodec::ReadUInt32(data, 0);
        parent.ResetSessionTimer();

        auto& server = parent;
        parent.NotifyObservers([&server, firmwareSize](auto& observer)
            {
                observer.OnBeginUpgrade(firmwareSize, [&server](FwuError status, uint16_t pageSize)
                    {
                        server.SendBeginResponse(status, pageSize);
                        server.SendCommandAck(fwuBeginUpgradeId, CanAckStatus::success);
                    });
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
        {
            parent.SendCommandAck(fwuDataBlockId, CanAckStatus::invalidPayload);
            return;
        }

        auto blockIndex = CanFrameCodec::ReadUInt16(data, 0);
        parent.ResetSessionTimer();

        hal::Can::Message payload;
        for (std::size_t i = 2; i < data.size(); ++i)
            payload.push_back(data[i]);

        auto& server = parent;
        parent.NotifyObservers([&server, blockIndex, payload](auto& observer)
            {
                observer.OnDataBlock(blockIndex, payload, [&server, blockIndex](FwuError status)
                    {
                        server.SendDataBlockAck(status, blockIndex);
                        server.SendCommandAck(fwuDataBlockId, CanAckStatus::success);
                    });
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
        {
            parent.SendCommandAck(fwuVerifyId, CanAckStatus::invalidPayload);
            return;
        }

        auto expectedCrc32 = CanFrameCodec::ReadUInt32(data, 0);
        parent.StopSessionTimer();

        auto& server = parent;
        parent.NotifyObservers([&server, expectedCrc32](auto& observer)
            {
                observer.OnVerify(expectedCrc32, [&server](FwuError status)
                    {
                        server.SendVerifyResponse(status);
                        server.SendCommandAck(fwuVerifyId, CanAckStatus::success);
                    });
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

        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnActivate([&server](FwuError status)
                    {
                        server.SendActivateResponse(status);
                        server.SendCommandAck(fwuActivateId, CanAckStatus::success);
                    });
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

        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnAbort([&server]()
                    {
                        server.SendCommandAck(fwuAbortId, CanAckStatus::success);
                    });
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
        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnQueryProgress([&server](FwuState state, uint16_t blocksReceived, uint16_t totalBlocks)
                    {
                        server.SendProgressResponse(state, blocksReceived, totalBlocks);
                        server.SendCommandAck(fwuQueryProgressId, CanAckStatus::success);
                    });
            });
    }
}
