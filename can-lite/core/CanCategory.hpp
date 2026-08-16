#pragma once

#include "can-lite/core/CanCategoryOutbound.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/IntrusiveList.hpp"
#include <cstddef>
#include <cstdint>

namespace services
{
    enum class CanDispatchResult : uint8_t
    {
        unknownMessageType,
        rejected,
        handled
    };

    // A message type handler is given the complete payload of a frame or of a
    // reassembled ISO-TP PDU. Returning false rejects the payload, which the
    // host answers with an invalidPayload acknowledgement.
    using CanMessageHandler = infra::Function<bool(infra::ConstByteRange payload)>;

    struct CanMessageTypeBinding
    {
        uint8_t messageType;
        CanMessageHandler handler;
    };

    // Owns the bounded storage for a category's message type bindings. A
    // category derives from this before CanCategoryServer/CanCategoryClient so
    // that the storage is constructed before the reference to it is taken.
    template<std::size_t Max>
    class CanCategoryHandlerStorage
    {
    protected:
        CanCategoryHandlerStorage() = default;
        CanCategoryHandlerStorage(const CanCategoryHandlerStorage&) = delete;
        CanCategoryHandlerStorage& operator=(const CanCategoryHandlerStorage&) = delete;
        ~CanCategoryHandlerStorage() = default;

        typename infra::BoundedVector<CanMessageTypeBinding>::template WithMaxSize<Max> messageTypeStorage;
    };

    class CanCategory
    {
    public:
        virtual uint8_t Id() const = 0;
        virtual bool RequiresSequenceValidation() const = 0;

        void AddMessageType(uint8_t messageType, const CanMessageHandler& handler);
        CanDispatchResult HandleMessage(uint8_t messageType, infra::ConstByteRange payload) const;

        // Called by the protocol host as part of registration and
        // unregistration; a category is never left holding a dangling handle.
        void AttachOutbound(CanCategoryOutbound& outbound);
        void DetachOutbound();

    protected:
        explicit CanCategory(infra::BoundedVector<CanMessageTypeBinding>& messageTypes);
        CanCategory(const CanCategory&) = delete;
        CanCategory& operator=(const CanCategory&) = delete;
        ~CanCategory() = default;

        CanCategoryOutbound& Outbound() const;

    private:
        infra::BoundedVector<CanMessageTypeBinding>& messageTypes;
        CanCategoryOutbound* outbound = &CanCategoryOutboundNull::Instance();
    };

    class CanCategoryServer
        : public CanCategory
        , public infra::IntrusiveList<CanCategoryServer>::NodeType
    {
    public:
        void SendCommandAck(uint8_t messageType, CanAckStatus status);

    protected:
        using CanCategory::CanCategory;
        ~CanCategoryServer() = default;
    };

    class CanCategoryClient
        : public CanCategory
        , public infra::IntrusiveList<CanCategoryClient>::NodeType
    {
    protected:
        using CanCategory::CanCategory;
        ~CanCategoryClient() = default;
    };
}
