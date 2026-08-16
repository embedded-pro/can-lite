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
        RegisterCategory(systemCategory);

        can.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                ProcessReceivedMessage(id, data);
            });
    }

    CanProtocolClient::SystemObserver::SystemObserver(CanSystemCategoryClient& subject, CanProtocolClient& client)
        : CanSystemCategoryClientObserver(subject)
        , client(client)
    {}

    void CanProtocolClient::SystemObserver::OnCommandAck(const CanCommandAck& ack)
    {
        if (ack.status != CanAckStatus::sequenceError)
            return;

        // The server rejected a command because its sequence did not match.
        // Without this the two ends drift apart for good, since the server does
        // not advance its counter on a mismatch while the client already has.
        auto outbound = client.FindOutbound(ack.category);
        if (outbound != nullptr)
            outbound->ResyncSequence(client.currentSourceNodeId, ack.expectedSequence);
    }

    void CanProtocolClient::SystemObserver::OnCategoryListResponse(infra::ConstByteRange categoryIds)
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
        really_assert(category.Id() <= canMaxCategoryId);
        really_assert(categoryCount < canMaxCategories);

        for (auto& existing : categories)
            really_assert(existing.Id() != category.Id());

        category.AttachOutbound(AllocateOutbound(category.Id()));
        categories.push_back(category);
        ++categoryCount;
    }

    void CanProtocolClient::UnregisterCategory(CanCategoryClient& category)
    {
        auto outbound = FindOutbound(category.Id());
        if (outbound != nullptr)
            outbound->Unbind();

        category.DetachOutbound();
        categories.erase(category);
        --categoryCount;
    }

    CanCategoryOutboundImpl* CanProtocolClient::FindOutbound(uint8_t categoryId)
    {
        for (auto& outbound : outbounds)
            if (outbound.IsBoundTo(categoryId))
                return &outbound;

        return nullptr;
    }

    CanCategoryOutboundImpl& CanProtocolClient::AllocateOutbound(uint8_t categoryId)
    {
        for (auto& outbound : outbounds)
            if (!outbound.IsBound())
            {
                outbound.Bind(transport, categoryId);
                return outbound;
            }

        // Unreachable: registration already limits the number of categories.
        really_assert(false);
        return outbounds.front();
    }

    CanSystemCategoryClient& CanProtocolClient::SystemCategory()
    {
        return systemCategory;
    }

    void CanProtocolClient::AttachIsoTpTransport(IsoTpTransport& isoTp)
    {
        isoTpTransport = &isoTp;
        isoTp.SetOnPduReceived([this](uint32_t rawId, infra::ConstByteRange pdu)
            {
                Dispatch(rawId, pdu);
            });
    }

    void CanProtocolClient::DiscoverCategories(uint16_t nodeId, const infra::Function<void(infra::ConstByteRange categoryIds)>& onDone)
    {
        pendingDiscoveryCallback = onDone;

        hal::Can::Message emptyPayload;
        transport.SendFrame(nodeId, CanPriority::command, canSystemCategoryId, canCategoryListRequestMessageTypeId, emptyPayload, [] {});
    }

    void CanProtocolClient::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!id.Is29BitId())
            return;

        uint32_t rawId = id.Get29BitId();

        if (isoTpTransport != nullptr &&
            isoTpTransport->ProcessFrame(rawId, data))
            return;

        Dispatch(rawId, infra::MakeRange(data));
    }

    void CanProtocolClient::Dispatch(uint32_t rawId, infra::ConstByteRange payload)
    {
        auto sourceNodeId = ExtractCanNodeId(rawId);
        auto categoryId = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        if (sourceNodeId != 0)
            MarkServerAlive(sourceNodeId);

        currentSourceNodeId = sourceNodeId;

        for (auto& category : categories)
        {
            if (category.Id() == categoryId)
            {
                category.HandleMessage(messageType, payload);
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
