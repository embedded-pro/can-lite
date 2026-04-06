#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
#include "gmock/gmock.h"
#include <cstdint>
#include <initializer_list>

namespace services::iso_tp::test
{
    inline hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
    {
        hal::Can::Message msg;
        for (uint8_t b : bytes)
            msg.push_back(b);
        return msg;
    }

    class MockSendFrame
    {
    public:
        MOCK_METHOD(void, Call, (const hal::Can::Message& frame, const infra::Function<void()>& onDone));
    };
}
