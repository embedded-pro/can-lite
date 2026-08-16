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
        , systemObserver(systemCategory, *this)
    {
        RegisterCategory(systemCategory);

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

    void CanProtocolServer::RegisterCategory(CanCategoryServer& category)
    {
        really_assert(category.Id() <= canMaxCategoryId);
        really_assert(categoryCount < canMaxCategories);

        for (auto& existing : categories)
            really_assert(existing.Id() != category.Id());

        category.AttachOutbound(AllocateOutbound(category.Id()));
        categories.push_back(category);
        ++categoryCount;
    }

    void CanProtocolServer::UnregisterCategory(CanCategoryServer& category)
    {
        auto outbound = FindOutbound(category.Id());
        if (outbound != nullptr)
            outbound->Unbind();

        category.DetachOutbound();
        categories.erase(category);
        --categoryCount;
    }

    void CanProtocolServer::AttachIsoTpTransport(IsoTpTransport& isoTp)
    {
        isoTpTransport = &isoTp;
        isoTp.SetOnPduReceived([this](uint32_t rawId, infra::ConstByteRange pdu)
            {
                Dispatch(rawId, pdu);
            });
    }

    void CanProtocolServer::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!id.Is29BitId())
            return;

        uint32_t rawId = id.Get29BitId();

        if (isoTpTransport != nullptr &&
            isoTpTransport->ProcessFrame(rawId, data))
            return;

        Dispatch(rawId, infra::MakeRange(data));
    }

    void CanProtocolServer::Dispatch(uint32_t rawId, infra::ConstByteRange payload)
    {
        uint16_t targetNodeId = ExtractCanNodeId(rawId);

        if (targetNodeId != config.nodeId && targetNodeId != canBroadcastNodeId)
            return;

        if (!CheckAndIncrementRate())
            return;

        auto categoryId = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        CanCategoryServer* category = FindCategory(categoryId);
        if (category == nullptr)
            return;

        auto outbound = FindOutbound(categoryId);
        if (outbound == nullptr)
            return;

        uint8_t correlation = 0;

        if (category->RequiresSequenceValidation())
        {
            if (payload.empty())
            {
                outbound->SendAckWith(config.nodeId, messageType, CanAckStatus::invalidPayload, 0, 0);
                return;
            }

            correlation = payload.front();

            auto validation = outbound->ValidateSequence(targetNodeId, correlation);
            if (!validation.accepted)
            {
                // The acknowledgement carries the sequence the server expects,
                // so a peer that lost a frame can resynchronise instead of
                // being locked out for good.
                outbound->SendAckWith(config.nodeId, messageType, CanAckStatus::sequenceError,
                    correlation, validation.expected);
                return;
            }
        }

        outbound->BeginRequest(targetNodeId, correlation);

        switch (category->HandleMessage(messageType, payload))
        {
            case CanDispatchResult::unknownMessageType:
                outbound->SendAckWith(config.nodeId, messageType, CanAckStatus::unknownCommand, correlation, 0);
                break;
            case CanDispatchResult::rejected:
                outbound->SendAckWith(config.nodeId, messageType, CanAckStatus::invalidPayload, correlation, 0);
                break;
            case CanDispatchResult::handled:
                break;
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

    CanCategoryOutboundImpl* CanProtocolServer::FindOutbound(uint8_t categoryId)
    {
        for (auto& outbound : outbounds)
            if (outbound.IsBoundTo(categoryId))
                return &outbound;

        return nullptr;
    }

    CanCategoryOutboundImpl& CanProtocolServer::AllocateOutbound(uint8_t categoryId)
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

    void CanProtocolServer::SendHeartbeat()
    {
        hal::Can::Message msg;
        msg.push_back(canProtocolVersion);

        transport.SendFrame(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, msg, [] {});
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

        // Registration guarantees at most canMaxCategories categories, so the
        // list always fits in one frame and never needs truncating.
        for (auto& category : categories)
            msg.push_back(category.Id());

        transport.SendFrame(CanPriority::response, canSystemCategoryId, canCategoryListResponseMessageTypeId, msg, [] {});
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

    CanFrameTransport& CanProtocolServer::Transport()
    {
        return transport;
    }
}
