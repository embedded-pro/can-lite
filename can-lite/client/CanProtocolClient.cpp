#include "can-lite/client/CanProtocolClient.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    CanProtocolClient::CanProtocolClient(hal::Can& can)
        : CanProtocolClient(can, Config{})
    {}

    CanProtocolClient::CanProtocolClient(hal::Can& can, const Config& config)
        : can(can)
        , config(config)
        , transport(can, 0)
        , systemCategory(transport, *this)
    {
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

    CanProtocolClient::~CanProtocolClient()
    {
        can.ReceiveData(nullptr);
        transport.ClearOnSendNotification();
    }

    bool CanProtocolClient::RegisterCategory(CanCategoryClient& category)
    {
        if (categories.size() >= canMaxRegisteredCategories)
            return false;

        for (auto& existing : categories)
            if (existing.Id() == category.Id())
                return false;

        categories.push_back(category);
        return true;
    }

    void CanProtocolClient::UnregisterCategory(CanCategoryClient& category)
    {
        categories.erase(category);
    }

    CanSystemCategoryClient& CanProtocolClient::SystemCategory()
    {
        return systemCategory;
    }

    CanFrameTransport& CanProtocolClient::Transport()
    {
        return transport;
    }

    void CanProtocolClient::AttachIsoTpTransport(IsoTpTransport& isoTp)
    {
        isoTpTransport = &isoTp;
        isoTp.SetOnPduReceived([this](uint32_t rawId, infra::ConstByteRange pdu)
            {
                DispatchPdu(rawId, pdu);
            });
    }

    void CanProtocolClient::DetachIsoTpTransport()
    {
        if (isoTpTransport == nullptr)
            return;

        isoTpTransport->SetOnPduReceived(nullptr);
        isoTpTransport = nullptr;
    }

    void CanProtocolClient::DispatchPdu(uint32_t rawId, infra::ConstByteRange pdu)
    {
        auto sourceNodeId = ExtractCanNodeId(rawId);
        if (sourceNodeId != 0)
            MarkServerAlive(sourceNodeId);

        auto categoryId = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        HandleCommandAckFrame(sourceNodeId, categoryId, messageType, pdu);
        HandleCategoryListFrame(sourceNodeId, categoryId, messageType, pdu);

        for (auto& category : categories)
        {
            if (category.Id() == categoryId)
            {
                category.HandlePduMessage(messageType, pdu);
                return;
            }
        }
    }

    bool CanProtocolClient::DiscoverCategories(uint16_t nodeId, const infra::Function<void(std::optional<infra::ConstByteRange> categoryIds)>& onDone)
    {
        if (discoveryNode || nodeId == canBroadcastNodeId)
            return false;

        discoveryNode = nodeId;
        discoveryDone = onDone;
        discoveryTimer.Start(config.discoveryTimeout, [this]()
            {
                CompleteDiscovery(std::nullopt);
            });

        if (systemCategory.RequestCategoryList(nodeId))
            return true;

        discoveryTimer.Cancel();
        discoveryNode.reset();
        discoveryDone = nullptr;
        return false;
    }

    void CanProtocolClient::HandleCategoryListFrame(uint16_t sourceNodeId, uint8_t categoryId, uint8_t messageType, infra::ConstByteRange payload)
    {
        if (categoryId != canSystemCategoryId || messageType != canCategoryListResponseMessageTypeId)
            return;

        if (discoveryNode == sourceNodeId)
            CompleteDiscovery(payload);
    }

    void CanProtocolClient::CompleteDiscovery(std::optional<infra::ConstByteRange> categoryIds)
    {
        discoveryTimer.Cancel();
        auto onDone = discoveryDone;
        discoveryDone = nullptr;
        discoveryNode.reset();
        onDone(categoryIds);
    }

    uint8_t CanProtocolClient::ReserveSequence(uint16_t nodeId, uint8_t category, uint8_t messageType)
    {
        auto previousEvictIndex = nextSequenceEvictIndex;
        auto& state = ClaimSequenceSlot(nodeId);
        lastReservation = Reservation{ &state, 0, state, state.ackTimer.NextTrigger(), previousEvictIndex };

        if (!state.occupied || state.nodeId != nodeId)
            static_cast<SequenceRecord&>(state) = SequenceRecord{ nodeId, 0, true };

        lastReservation->sequence = state.sequenceCounter++;
        state.awaitingAck = true;
        state.awaitingCategory = category;
        state.awaitingMessageType = messageType;
        AwaitAck(state, state.ackTimer.Now() + config.commandAckTimeout);
        return lastReservation->sequence;
    }

    void CanProtocolClient::CancelReservation(uint16_t nodeId, uint8_t sequence)
    {
        really_assert(lastReservation && lastReservation->state->nodeId == nodeId && lastReservation->sequence == sequence);

        auto& reservation = *lastReservation;
        auto& state = *reservation.state;
        static_cast<SequenceRecord&>(state) = reservation.previous;
        nextSequenceEvictIndex = reservation.previousEvictIndex;

        if (state.awaitingAck)
            AwaitAck(state, reservation.previousAckDeadline);
        else
            state.ackTimer.Cancel();

        lastReservation.reset();
    }

    void CanProtocolClient::ReturnSequence(uint16_t nodeId, uint8_t sequence)
    {
        for (auto& state : serverStates)
        {
            if (state.occupied && state.nodeId == nodeId && state.sequenceCounter == static_cast<uint8_t>(sequence + 1))
            {
                state.sequenceCounter = sequence;
                return;
            }
        }
    }

    CanProtocolClient::PerServerState& CanProtocolClient::ClaimSequenceSlot(uint16_t nodeId)
    {
        for (auto& state : serverStates)
            if (state.occupied && state.nodeId == nodeId)
                return state;

        for (auto& state : serverStates)
            if (!state.occupied)
                return state;

        auto& evicted = serverStates[nextSequenceEvictIndex];
        nextSequenceEvictIndex = static_cast<uint8_t>((nextSequenceEvictIndex + 1) % serverStates.size());
        return evicted;
    }

    void CanProtocolClient::AwaitAck(PerServerState& state, infra::TimePoint deadline)
    {
        state.ackTimer.Start(deadline, [this, nodeId = state.nodeId]()
            {
                HandleCommandAckTimeout(nodeId);
            });
    }

    void CanProtocolClient::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!id.Is29BitId())
            return;

        uint32_t rawId = id.Get29BitId();

        if (isoTpTransport != nullptr &&
            isoTpTransport->ProcessFrame(rawId, data))
            return;

        auto sourceNodeId = ExtractCanNodeId(rawId);
        auto categoryId = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        if (sourceNodeId != 0)
            MarkServerAlive(sourceNodeId);

        HandleCommandAckFrame(sourceNodeId, categoryId, messageType, infra::MakeRange(data));
        HandleCategoryListFrame(sourceNodeId, categoryId, messageType, infra::MakeRange(data));

        for (auto& category : categories)
        {
            if (category.Id() == categoryId)
            {
                category.HandleMessage(messageType, data);
                return;
            }
        }
    }

    void CanProtocolClient::HandleCommandAckFrame(uint16_t sourceNodeId, uint8_t categoryId, uint8_t messageType, infra::ConstByteRange payload)
    {
        if (categoryId != canSystemCategoryId || messageType != canCommandAckMessageTypeId)
            return;

        if (payload.size() < canCommandAckSize || sourceNodeId == 0)
            return;

        auto ackedCategory = payload[0];
        auto ackedMessageType = payload[1];
        auto status = static_cast<CanAckStatus>(payload[2]);

        ClearAwaitingAck(sourceNodeId, ackedCategory, ackedMessageType);

        if (status == CanAckStatus::sequenceError)
            ResyncSequence(sourceNodeId, payload[3]);
    }

    void CanProtocolClient::ResyncSequence(uint16_t nodeId, uint8_t expectedSequence)
    {
        for (auto& state : serverStates)
        {
            if (state.occupied && state.nodeId == nodeId)
            {
                state.sequenceCounter = expectedSequence;
                return;
            }
        }
    }

    void CanProtocolClient::ClearAwaitingAck(uint16_t nodeId, uint8_t category, uint8_t messageType)
    {
        for (auto& state : serverStates)
        {
            if (state.occupied && state.nodeId == nodeId && state.awaitingAck &&
                state.awaitingCategory == category && state.awaitingMessageType == messageType)
            {
                state.awaitingAck = false;
                state.ackTimer.Cancel();
                return;
            }
        }
    }

    void CanProtocolClient::HandleCommandAckTimeout(uint16_t nodeId)
    {
        for (auto& state : serverStates)
        {
            if (state.occupied && state.nodeId == nodeId && state.awaitingAck)
            {
                state.awaitingAck = false;
                auto category = state.awaitingCategory;
                auto messageType = state.awaitingMessageType;
                NotifyObservers([nodeId, category, messageType](auto& obs)
                    {
                        obs.OnCommandAckTimeout(nodeId, category, messageType);
                    });
                return;
            }
        }
    }

    void CanProtocolClient::SendHeartbeat()
    {
        hal::Can::Message msg;
        msg.push_back(canProtocolVersion);

        transport.SendFrame(canBroadcastNodeId, CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, msg, [](bool) {});

        ResetHeartbeatTimer();
    }

    void CanProtocolClient::ResetHeartbeatTimer()
    {
        heartbeatTimer.Start(config.heartbeatInterval, [this]()
            {
                SendHeartbeat();
            });
    }

    void CanProtocolClient::MarkServerAlive(uint16_t nodeId)
    {
        for (auto& entry : serverLiveness)
        {
            if (entry.occupied && entry.nodeId == nodeId)
            {
                entry.timeoutTimer.Start(config.serverTimeout, [this, nodeId]()
                    {
                        HandleServerTimeout(nodeId);
                    });
                return;
            }
        }

        for (auto& entry : serverLiveness)
        {
            if (!entry.occupied)
            {
                entry.occupied = true;
                entry.nodeId = nodeId;
                NotifyObservers([nodeId](auto& obs)
                    {
                        obs.OnServerOnline(nodeId);
                    });
                entry.timeoutTimer.Start(config.serverTimeout, [this, nodeId]()
                    {
                        HandleServerTimeout(nodeId);
                    });
                return;
            }
        }

        // All slots are tracking other servers: evict the oldest tracked entry
        // (round robin) instead of silently dropping the newly observed node.
        auto& evicted = serverLiveness[nextLivenessEvictIndex];
        nextLivenessEvictIndex = static_cast<uint8_t>((nextLivenessEvictIndex + 1) % serverLiveness.size());
        evicted.timeoutTimer.Cancel();
        auto evictedNodeId = evicted.nodeId;
        NotifyObservers([evictedNodeId](auto& obs)
            {
                obs.OnServerOffline(evictedNodeId);
            });
        evicted.nodeId = nodeId;
        NotifyObservers([nodeId](auto& obs)
            {
                obs.OnServerOnline(nodeId);
            });
        evicted.timeoutTimer.Start(config.serverTimeout, [this, nodeId]()
            {
                HandleServerTimeout(nodeId);
            });
    }

    void CanProtocolClient::HandleServerTimeout(uint16_t nodeId)
    {
        for (auto& entry : serverLiveness)
        {
            if (entry.occupied && entry.nodeId == nodeId)
            {
                entry.occupied = false;
                NotifyObservers([nodeId](auto& obs)
                    {
                        obs.OnServerOffline(nodeId);
                    });
                return;
            }
        }
    }
}
