#pragma once

#include "can-lite/core/CanMessageType.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/IntrusiveList.hpp"
#include <cstdint>

namespace services
{
    class CanCommandAcknowledger
    {
    public:
        virtual void SendCommandAck(uint8_t category, uint8_t messageType, CanAckStatus status) = 0;

    protected:
        CanCommandAcknowledger() = default;
        CanCommandAcknowledger(const CanCommandAcknowledger&) = delete;
        CanCommandAcknowledger& operator=(const CanCommandAcknowledger&) = delete;
        ~CanCommandAcknowledger() = default;
    };

    class CanCategory
    {
    public:
        virtual uint8_t Id() const = 0;
        virtual bool RequiresSequenceValidation() const = 0;

        void AddMessageType(CanMessageType& messageType);
        bool HandleMessage(uint8_t messageType, const hal::Can::Message& data);
        bool HandlePduMessage(uint8_t messageType, infra::ConstByteRange pdu);

    protected:
        CanCategory() = default;
        CanCategory(const CanCategory&) = delete;
        CanCategory& operator=(const CanCategory&) = delete;
        ~CanCategory() = default;

    private:
        infra::IntrusiveList<CanMessageType> messageTypes;
    };

    class CanCategoryServer
        : public CanCategory
        , public infra::IntrusiveList<CanCategoryServer>::NodeType
    {
    public:
        bool RequiresSequenceValidation() const override;

        void SetAcknowledger(CanCommandAcknowledger& acknowledger);
        void SendCommandAck(uint8_t messageType, CanAckStatus status);

    protected:
        CanCategoryServer() = default;
        ~CanCategoryServer() = default;

    private:
        CanCommandAcknowledger* acknowledger = nullptr;
    };

    class CanCategoryClient
        : public CanCategory
        , public infra::IntrusiveList<CanCategoryClient>::NodeType
    {
    public:
        bool RequiresSequenceValidation() const override;

    protected:
        CanCategoryClient() = default;
        ~CanCategoryClient() = default;
    };
}
