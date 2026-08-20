#include "can-lite/server/CanProtocolServer.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    CanProtocolServer::CanProtocolServer(hal::Can& can, const Config& config)
        : config(config)
        , transport(can, config.nodeId)
        , rateResetTimer(std::chrono::seconds(1), [this]()
              {
                  ResetRateCounter();
              })
        , systemCategory(transport)
        , systemObserver(systemCategory, *this)
    {
        really_assert(config.nodeId != canBroadcastNodeId);

        systemCategory.SetAcknowledger(*this);
        categories.push_back(systemCategory);

        can.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                ProcessReceivedMessage(id, data);
            });

        transport.SetOnSendNotification([this]()
            {
                ResetHeartbeatTimer();
            });

        ResetHeartbeatTimer();
    }

    CanProtocolServer::SystemObserver::SystemObserver(CanSystemCategoryServer& subject, CanProtocolServer& server)
        : CanSystemCategoryServerObserver(subject)
        , server(server)
    {}

    void CanProtocolServer::SystemObserver::OnHeartbeatReceived(uint8_t)
    {
        server.NotifyObservers([](auto& observer)
            {
                observer.Online();
            });
    }

    void CanProtocolServer::SystemObserver::OnStatusRequest()
    {
        server.SendHeartbeat();
    }

    void CanProtocolServer::SystemObserver::OnCategoryListRequest()
    {
        server.SendCategoryList();
    }

    bool CanProtocolServer::RegisterCategory(CanCategoryServer& category)
    {
        if (categories.size() >= canMaxRegisteredCategories)
            return false;

        for (auto& existing : categories)
            if (existing.Id() == category.Id())
                return false;

        category.SetAcknowledger(*this);
        categories.push_back(category);
        return true;
    }

    void CanProtocolServer::UnregisterCategory(CanCategoryServer& category)
    {
        categories.erase(category);
    }

    void CanProtocolServer::AttachIsoTpTransport(IsoTpTransport& isoTp)
    {
        isoTpTransport = &isoTp;
        isoTp.SetOnPduReceived([this](uint32_t rawId, infra::ConstByteRange pdu)
            {
                DispatchPdu(rawId, pdu);
            });
        isoTp.SetOnAbort([this](uint32_t dataId, iso_tp::AbortReason)
            {
                isoTpTransport->ReleaseChannel(dataId);
            });
    }

    void CanProtocolServer::DispatchPdu(uint32_t rawId, infra::ConstByteRange pdu)
    {
        auto nodeId = ExtractCanNodeId(rawId);
        if (nodeId != config.nodeId && nodeId != canBroadcastNodeId)
            return;

        MarkClientAlive();

        if (!CheckAndIncrementRate())
            return;

        auto categoryId = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        if (!IsCommandMessageType(messageType))
            return;

        CanCategoryServer* category = FindCategory(categoryId);
        if (category == nullptr)
            return;

        if (category->RequiresSequenceValidation())
        {
            if (pdu.empty())
            {
                SendCommandAck(categoryId, messageType, CanAckStatus::invalidPayload);
                return;
            }

            uint8_t sequenceNumber = pdu[0];
            auto validation = ValidateSequence(sequenceNumber);
            if (!validation.accepted)
            {
                SendCommandAck(categoryId, messageType, CanAckStatus::sequenceError, validation.expected);
                return;
            }
        }

        if (!category->HandlePduMessage(messageType, pdu))
        {
            SendCommandAck(categoryId, messageType, CanAckStatus::unknownCommand);
            return;
        }
    }

    void CanProtocolServer::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!id.Is29BitId())
            return;

        uint32_t rawId = id.Get29BitId();

        if (isoTpTransport != nullptr &&
            isoTpTransport->ProcessFrame(rawId, data))
            return;

        uint16_t targetNodeId = ExtractCanNodeId(rawId);

        if (targetNodeId != config.nodeId && targetNodeId != canBroadcastNodeId)
            return;

        MarkClientAlive();

        if (!CheckAndIncrementRate())
            return;

        auto categoryId = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        if (!IsCommandMessageType(messageType))
            return;

        CanCategoryServer* category = FindCategory(categoryId);
        if (category == nullptr)
            return;

        if (category->RequiresSequenceValidation())
        {
            if (data.empty())
            {
                SendCommandAck(categoryId, messageType, CanAckStatus::invalidPayload);
                return;
            }

            uint8_t sequenceNumber = data[0];
            auto validation = ValidateSequence(sequenceNumber);
            if (!validation.accepted)
            {
                SendCommandAck(categoryId, messageType, CanAckStatus::sequenceError, validation.expected);
                return;
            }
        }

        if (!category->HandleMessage(messageType, data))
        {
            SendCommandAck(categoryId, messageType, CanAckStatus::unknownCommand);
            return;
        }
    }

    CanCategoryServer* CanProtocolServer::FindCategory(uint8_t categoryId)
    {
        for (auto& category : categories)
        {
            if (category.Id() == categoryId)
                return &category;
        }

        return nullptr;
    }

    void CanProtocolServer::SendCommandAck(uint8_t category, uint8_t commandType, CanAckStatus status)
    {
        SendCommandAck(category, commandType, status, 0);
    }

    void CanProtocolServer::SendCommandAck(uint8_t category, uint8_t commandType, CanAckStatus status, uint8_t expectedSequence)
    {
        hal::Can::Message msg;
        msg.push_back(category);
        msg.push_back(commandType);
        msg.push_back(static_cast<uint8_t>(status));
        msg.push_back(expectedSequence);

        transport.SendFrame(CanPriority::response, canSystemCategoryId, canCommandAckMessageTypeId, msg, [](bool) {});
    }

    void CanProtocolServer::SendHeartbeat()
    {
        hal::Can::Message msg;
        msg.push_back(canProtocolVersion);

        transport.SendFrame(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, msg, [](bool) {});
    }

    void CanProtocolServer::ResetHeartbeatTimer()
    {
        heartbeatTimer.Start(config.heartbeatInterval, [this]()
            {
                SendHeartbeat();
            });
    }

    void CanProtocolServer::SendCategoryList()
    {
        hal::Can::Message msg;

        for (auto& category : categories)
            if (!msg.full())
                msg.push_back(category.Id());

        transport.SendFrame(CanPriority::response, canSystemCategoryId, canCategoryListResponseMessageTypeId, msg, [](bool) {});
    }

    void CanProtocolServer::ResetRateCounter()
    {
        messageCountThisPeriod = 0;
    }

    bool CanProtocolServer::CheckAndIncrementRate()
    {
        if (messageCountThisPeriod >= config.maxMessagesPerSecond)
            return false;

        ++messageCountThisPeriod;
        return true;
    }

    CanProtocolServer::SequenceValidationResult CanProtocolServer::ValidateSequence(uint8_t sequenceNumber)
    {
        if (!sequenceInitialized)
        {
            sequenceInitialized = true;
            lastSequenceNumber = sequenceNumber;
            return { true, sequenceNumber };
        }

        auto expected = static_cast<uint8_t>(lastSequenceNumber + 1);
        if (sequenceNumber != expected)
            return { false, expected };

        lastSequenceNumber = sequenceNumber;
        return { true, sequenceNumber };
    }

    void CanProtocolServer::MarkClientAlive()
    {
        clientOnline = true;
        clientLivenessTimer.Start(config.clientTimeout, [this]()
            {
                HandleClientTimeout();
            });
    }

    void CanProtocolServer::HandleClientTimeout()
    {
        if (!clientOnline)
            return;

        clientOnline = false;
        NotifyObservers([](auto& observer)
            {
                observer.Offline();
            });
    }

    CanFrameTransport& CanProtocolServer::Transport()
    {
        return transport;
    }
}
