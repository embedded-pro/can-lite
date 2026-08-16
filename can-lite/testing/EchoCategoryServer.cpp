#include "can-lite/testing/EchoCategoryServer.hpp"

namespace services
{
    EchoCategoryServer::EchoCategoryServer(uint8_t categoryId, bool requiresSequenceValidation)
        : CanCategoryServer(messageTypeStorage)
        , categoryId(categoryId)
        , requiresSequenceValidation(requiresSequenceValidation)
    {
        AddMessageType(echoRequestMessageTypeId, [this](infra::ConstByteRange payload)
            {
                return HandleEchoRequest(payload);
            });

        AddMessageType(echoValidatedRequestMessageTypeId, [this](infra::ConstByteRange payload)
            {
                return HandleValidatedRequest(payload);
            });
    }

    uint8_t EchoCategoryServer::Id() const
    {
        return categoryId;
    }

    bool EchoCategoryServer::RequiresSequenceValidation() const
    {
        return requiresSequenceValidation;
    }

    uint32_t EchoCategoryServer::RequestCount() const
    {
        return requestCount;
    }

    uint32_t EchoCategoryServer::ValidatedRequestCount() const
    {
        return validatedRequestCount;
    }

    uint32_t EchoCategoryServer::RejectedRequestCount() const
    {
        return rejectedRequestCount;
    }

    bool EchoCategoryServer::HandleEchoRequest(infra::ConstByteRange payload)
    {
        ++requestCount;

        hal::Can::Message reply;
        for (auto byte : infra::Head(payload, reply.max_size()))
            reply.push_back(byte);

        Outbound().Send(CanPriority::response, echoReplyMessageTypeId, reply);

        NotifyObservers([payload](auto& observer)
            {
                observer.OnEchoRequest(payload);
            });

        return true;
    }

    bool EchoCategoryServer::HandleValidatedRequest(infra::ConstByteRange payload)
    {
        if (payload.size() < echoMinimumValidatedPayloadSize)
        {
            ++rejectedRequestCount;
            return false;
        }

        ++validatedRequestCount;

        NotifyObservers([payload](auto& observer)
            {
                observer.OnValidatedRequest(payload);
            });

        return true;
    }
}
