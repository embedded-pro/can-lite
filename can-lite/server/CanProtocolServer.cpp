#include "can-lite/server/CanProtocolServer.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    namespace
    {
        bool IsEmergency(uint32_t rawId)
        {
            return ExtractCanPriority(rawId) == CanPriority::emergency;
        }

        bool ConsumeBudget(uint16_t& used, uint16_t limit, uint32_t& refusals)
        {
            if (used >= limit)
            {
                ++refusals;
                return false;
            }

            ++used;
            return true;
        }
    }

    CanProtocolServer::CanProtocolServer(hal::Can& can, const Config& config)
        : can(can)
        , config(config)
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

    CanProtocolServer::~CanProtocolServer()
    {
        can.ReceiveData(nullptr);
        transport.ClearOnSendNotification();
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

    bool CanProtocolServer::UnregisterCategory(CanCategoryServer& category)
    {
        if (&category == &systemCategory)
            return false;

        for (auto& existing : categories)
        {
            if (&existing == &category)
            {
                categories.erase(category);
                category.ClearAcknowledger();
                return true;
            }
        }

        return false;
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

    void CanProtocolServer::DetachIsoTpTransport()
    {
        if (isoTpTransport == nullptr)
            return;

        isoTpTransport->SetOnPduReceived(nullptr);
        isoTpTransport->SetOnAbort(nullptr);
        isoTpTransport = nullptr;
    }

    void CanProtocolServer::DispatchPdu(uint32_t rawId, infra::ConstByteRange pdu)
    {
        if (!IsAddressedToThisNode(rawId))
            return;

        if (auto* category = AdmitCommand(rawId, pdu))
        {
            auto checkpoint = CommitSequence(*category, pdu);
            Conclude(rawId, checkpoint, category->HandlePduMessage(ExtractCanMessageType(rawId), pdu));
        }
    }

    void CanProtocolServer::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!id.Is29BitId())
            return;

        uint32_t rawId = id.Get29BitId();

        if (!IsAddressedToThisNode(rawId) || !CheckAndIncrementRate(rawId))
            return;

        if (isoTpTransport != nullptr && isoTpTransport->ProcessFrame(rawId, data))
            return;

        if (auto* category = AdmitCommand(rawId, infra::MakeRange(data)))
        {
            auto checkpoint = CommitSequence(*category, infra::MakeRange(data));
            Conclude(rawId, checkpoint, category->HandleMessage(ExtractCanMessageType(rawId), data));
        }
    }

    bool CanProtocolServer::IsAddressedToThisNode(uint32_t rawId) const
    {
        auto nodeId = ExtractCanNodeId(rawId);
        return nodeId == config.nodeId || nodeId == canBroadcastNodeId;
    }

    CanCategoryServer* CanProtocolServer::AdmitCommand(uint32_t rawId, infra::ConstByteRange payload)
    {
        auto* category = IsCommandMessageType(ExtractCanMessageType(rawId)) ? FindCategory(ExtractCanCategory(rawId)) : nullptr;

        if (category == nullptr)
        {
            ++counters.invalidFrames;
            return nullptr;
        }

        if (!category->RequiresSequenceValidation())
            return category;

        if (payload.empty())
        {
            RejectCommand(rawId, CanAckStatus::invalidPayload, 0);
            return nullptr;
        }

        if (!IsEmergency(rawId) && !IsExpectedSequence(payload[0]))
        {
            RejectCommand(rawId, CanAckStatus::sequenceError, ExpectedSequence());
            return nullptr;
        }

        return category;
    }

    void CanProtocolServer::Conclude(uint32_t rawId, const SequenceCheckpoint& checkpoint, CanDispatchResult result)
    {
        switch (result)
        {
            case CanDispatchResult::handled:
                AcceptCommand(rawId);
                break;
            case CanDispatchResult::rejected:
                RestoreSequence(checkpoint);
                RejectCommand(rawId, CanAckStatus::invalidPayload, 0);
                break;
            case CanDispatchResult::unknownMessageType:
                RestoreSequence(checkpoint);
                RejectCommand(rawId, CanAckStatus::unknownCommand, 0);
                break;
        }
    }

    void CanProtocolServer::AcceptCommand(uint32_t rawId)
    {
        if (IsEmergency(rawId))
            ++counters.emergencyAdmitted;

        MarkClientAlive();
    }

    void CanProtocolServer::RejectCommand(uint32_t rawId, CanAckStatus status, uint8_t expectedSequence)
    {
        ++counters.invalidFrames;
        SendCommandAck(ExtractCanCategory(rawId), ExtractCanMessageType(rawId), status, expectedSequence);
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

        ResetHeartbeatTimer();
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
        emergencyMessageCountThisPeriod = 0;
    }

    bool CanProtocolServer::CheckAndIncrementRate(uint32_t rawId)
    {
        if (IsEmergency(rawId))
            return ConsumeBudget(emergencyMessageCountThisPeriod, config.maxEmergencyMessagesPerSecond, counters.emergencyRateLimited);

        return ConsumeBudget(messageCountThisPeriod, config.maxMessagesPerSecond, counters.rateLimited);
    }

    bool CanProtocolServer::IsExpectedSequence(uint8_t sequenceNumber) const
    {
        return !sequenceInitialized || sequenceNumber == ExpectedSequence();
    }

    uint8_t CanProtocolServer::ExpectedSequence() const
    {
        return static_cast<uint8_t>(lastSequenceNumber + 1);
    }

    CanProtocolServer::SequenceCheckpoint CanProtocolServer::CommitSequence(const CanCategoryServer& category, infra::ConstByteRange payload)
    {
        SequenceCheckpoint checkpoint{ lastSequenceNumber, sequenceInitialized };

        if (category.RequiresSequenceValidation())
        {
            lastSequenceNumber = payload[0];
            sequenceInitialized = true;
        }

        return checkpoint;
    }

    void CanProtocolServer::RestoreSequence(const SequenceCheckpoint& checkpoint)
    {
        lastSequenceNumber = checkpoint.lastSequenceNumber;
        sequenceInitialized = checkpoint.sequenceInitialized;
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

    const CanProtocolServer::Counters& CanProtocolServer::Statistics() const
    {
        return counters;
    }
}
