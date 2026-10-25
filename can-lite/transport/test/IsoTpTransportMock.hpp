#pragma once

#include "can-lite/transport/IsoTpTransport.hpp"
#include "gmock/gmock.h"

namespace services
{
    class IsoTpTransportMock
        : public IsoTpTransport
    {
    public:
        MOCK_METHOD(bool, RegisterReceiveChannel, (uint32_t dataId, uint32_t fcId), (override));
        MOCK_METHOD(void, ReleaseChannel, (uint32_t dataId), (override));
        MOCK_METHOD(bool, SendPdu, (uint32_t dataId, uint32_t fcId, infra::ConstByteRange pdu, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(bool, ProcessFrame, (uint32_t canId, const hal::Can::Message& frame), (override));
        MOCK_METHOD(void, SetOnPduReceived, (infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> callback), (override));
        MOCK_METHOD(void, SetOnAbort, (infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> callback), (override));
    };
}
