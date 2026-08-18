#include "can-lite/client/CanProtocolClient.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    CanProtocolClient::CanProtocolClient(hal::Can& can)
        : CanProtocolClient(can, Config{})
    {}

    CanProtocolClient::CanProtocolClient(hal::Can& can, const Config& config)
        : config(config)
        , transport(can, 0)
        , systemCategory(transport, *this)
        , systemObserver(systemCategory, *this)
    {
        categories.push_back(systemCategory);

        can.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                ProcessReceivedMessage(id, data);
            });
    }

    CanProtocolClient::SystemObserver::SystemObserver(CanSystemCategoryClient& subject, CanProtocolClient& client)
        : CanSystemCategoryClientObserver(subject)
        , client(client)
    {}

    void CanProtocolClient::SystemObserver::OnCategoryListResponse(const hal::Can::Message& categoryIds)
    {
        if (client.pendingDiscoveryCallback)
        {
            auto callback = client.pendingDiscoveryCallback;
            client.pendingDiscoveryCallback = nullptr;
            callback(categoryIds);
        }
    }

    void CanProtocolClient::RegisterCategory(CanCategoryClient& category)
    {
        for (auto& existing : categories)
            really_assert(existing.Id() != category.Id());

        categories.push_back(category);
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
        isoTp.SetOnAbort([this](uint32_t dataId, iso_tp::AbortReason)
            {
                isoTpTransport->ReleaseChannel(dataId);
            });
    }

    void CanProtocolClient::DispatchPdu(uint32_t rawId, infra::ConstByteRange pdu)
    {
        auto sourceNodeId = ExtractCanNodeId(rawId);
        if (sourceNodeId != 0)
            MarkServerAlive(sourceNodeId);

        auto categoryId = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        for (auto& category : categories)
        {
            if (category.Id() == categoryId)
            {
                category.HandlePduMessage(messageType, pdu);
                return;
            }
        }
    }

    void CanProtocolClient::DiscoverCategories(uint16_t nodeId, const infra::Function<void(const hal::Can::Message&)>& onDone)
    {
        pendingDiscoveryCallback = onDone;

        hal::Can::Message emptyPayload;
        transport.SendFrame(nodeId, CanPriority::command, canSystemCategoryId, canCategoryListRequestMessageTypeId, emptyPayload, [](bool) {});
    }

    uint8_t CanProtocolClient::PeekSequence(uint16_t nodeId)
    {
        for (auto& state : serverStates)
        {
            if (state.occupied && state.nodeId == nodeId)
                return state.sequenceCounter;
        }

        for (auto& state : serverStates)
        {
            if (!state.occupied)
            {
                state.occupied = true;
                state.nodeId = nodeId;
                state.sequenceCounter = 0;
                return 0;
            }
        }

        // All slots are tracking other servers: evict the oldest tracked slot
        // (round robin) instead of hard-faulting. The evicted server's sequence
        // counter restarts at 0 and resynchronizes via the server's sequenceError
        // response, the same recovery path used for any other counter mismatch.
        auto& evicted = serverStates[nextSequenceEvictIndex];
        nextSequenceEvictIndex = static_cast<uint8_t>((nextSequenceEvictIndex + 1) % serverStates.size());
        evicted.nodeId = nodeId;
        evicted.sequenceCounter = 0;
        return 0;
    }

    void CanProtocolClient::CommitSequence(uint16_t nodeId)
    {
        for (auto& state : serverStates)
        {
            if (state.occupied && state.nodeId == nodeId)
            {
                ++state.sequenceCounter;
                return;
            }
        }

        really_assert(false);
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

        for (auto& category : categories)
        {
            if (category.Id() == categoryId)
            {
                category.HandleMessage(messageType, data);
                return;
            }
        }
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
