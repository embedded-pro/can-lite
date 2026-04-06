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

    void CanProtocolClient::AttachIsoTpTransport(IsoTpTransport& isoTp)
    {
        isoTpTransport = &isoTp;
        isoTp.SetOnPduReceived([this](uint32_t rawId, infra::ConstByteRange pdu)
            {
                DispatchPdu(rawId, pdu);
            });
    }

    void CanProtocolClient::DispatchPdu(uint32_t rawId, infra::ConstByteRange pdu)
    {
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
        transport.SendFrame(nodeId, CanPriority::command, canSystemCategoryId, canCategoryListRequestMessageTypeId, emptyPayload, [] {});
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

        really_assert(false);
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
                if (!entry.online)
                {
                    entry.online = true;
                    NotifyObservers([nodeId](auto& obs)
                        {
                            obs.OnServerOnline(nodeId);
                        });
                }
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
                entry.online = true;
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
    }

    void CanProtocolClient::HandleServerTimeout(uint16_t nodeId)
    {
        for (auto& entry : serverLiveness)
        {
            if (entry.occupied && entry.nodeId == nodeId)
            {
                entry.online = false;
                NotifyObservers([nodeId](auto& obs)
                    {
                        obs.OnServerOffline(nodeId);
                    });
                return;
            }
        }
    }
}
