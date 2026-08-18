#pragma once

#include "can-lite/core/CanMessageType.hpp"
#include <cstdint>

namespace services
{
    template<class Owner>
    class CanMessageHandler
        : public CanMessageType
    {
    public:
        using FrameHandler = void (Owner::*)(const hal::Can::Message&);
        using PduHandler = bool (Owner::*)(infra::ConstByteRange);

        CanMessageHandler(uint8_t id, Owner& owner, FrameHandler onFrame)
            : id{ id }
            , owner{ owner }
            , onFrame{ onFrame }
        {}

        CanMessageHandler(uint8_t id, Owner& owner, FrameHandler onFrame, PduHandler onPdu)
            : id{ id }
            , owner{ owner }
            , onFrame{ onFrame }
            , onPdu{ onPdu }
        {}

        uint8_t Id() const override
        {
            return id;
        }

        void Handle(const hal::Can::Message& data) override
        {
            (owner.*onFrame)(data);
        }

        bool HandlePdu(infra::ConstByteRange pdu) override
        {
            if (onPdu == nullptr)
                return CanMessageType::HandlePdu(pdu);

            return (owner.*onPdu)(pdu);
        }

    private:
        uint8_t id;
        Owner& owner;
        FrameHandler onFrame;
        PduHandler onPdu{ nullptr };
    };
}
