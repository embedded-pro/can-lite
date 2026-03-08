#include "can-lite/server/CanProtocolServer.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    CanProtocolServer::CanProtocolServer(hal::Can& can, const Config& config)
        : config(config)
        , transport(can, config.nodeId)
        , heartbeatTimer(config.heartbeatInterval, [this]()
              {
                  SendHeartbeat();
              })
        , rateResetTimer(std::chrono::seconds(1), [this]()
              {
                  ResetRateCounter();
              })
    {
        systemCategory.onHeartbeat = [this](uint8_t)
        {
            NotifyObservers([](auto& observer)
                {
                    observer.Online();
                });
        };

        systemCategory.onStatusRequest = [this]()
        {
            SendHeartbeat();
        };

        systemCategory.onCategoryListRequest = [this]()
        {
            SendCategoryList();
        };

        categories.push_back(systemCategory);

        can.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                ProcessReceivedMessage(id, data);
            });
    }

    void CanProtocolServer::RegisterCategory(CanCategory& category)
    {
        for (auto& existing : categories)
            really_assert(existing.Id() != category.Id());

        categories.push_back(category);
    }

    void CanProtocolServer::UnregisterCategory(CanCategory& category)
    {
        categories.erase(category);
    }

    void CanProtocolServer::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!id.Is29BitId())
            return;

        uint32_t rawId = id.Get29BitId();
        uint16_t targetNodeId = ExtractCanNodeId(rawId);

        if (targetNodeId != config.nodeId && targetNodeId != canBroadcastNodeId)
            return;

        if (!CheckAndIncrementRate())
            return;

        auto categoryId = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        CanCategory* category = FindCategory(categoryId);
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
            if (!ValidateSequence(sequenceNumber))
            {
                SendCommandAck(categoryId, messageType, CanAckStatus::sequenceError);
                return;
            }
        }

        if (!category->HandleMessage(messageType, data))
        {
            SendCommandAck(categoryId, messageType, CanAckStatus::unknownCommand);
        }
    }

    CanCategory* CanProtocolServer::FindCategory(uint8_t categoryId)
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
        hal::Can::Message msg;
        msg.push_back(category);
        msg.push_back(commandType);
        msg.push_back(static_cast<uint8_t>(status));

        transport.SendFrame(CanPriority::response, canSystemCategoryId, canCommandAckMessageTypeId, msg, [] {});
    }

    void CanProtocolServer::SendHeartbeat()
    {
        hal::Can::Message msg;
        msg.push_back(canProtocolVersion);

        transport.SendFrame(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, msg, [] {});
    }

    void CanProtocolServer::SendCategoryList()
    {
        hal::Can::Message msg;

        for (auto& category : categories)
            if (!msg.full())
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

    bool CanProtocolServer::ValidateSequence(uint8_t sequenceNumber)
    {
        if (!sequenceInitialized)
        {
            sequenceInitialized = true;
            lastSequenceNumber = sequenceNumber;
            return true;
        }

        auto expected = static_cast<uint8_t>(lastSequenceNumber + 1);
        if (sequenceNumber != expected)
            return false;

        lastSequenceNumber = sequenceNumber;
        return true;
    }
}
