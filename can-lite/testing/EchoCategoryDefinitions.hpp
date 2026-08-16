#pragma once

#include <cstddef>
#include <cstdint>

namespace services
{
    // The echo category is deliberately meaningless: it ascribes no application
    // semantics to its payload, so it is safe to ship inside can-lite while
    // still demonstrating everything a consumer-owned category has to do.
    //
    // It is a reference example, not a service. Give it any integrator-assigned
    // category ID in the custom range through its constructor.

    // Echoes whatever it receives back to the sender. Exercises dispatch,
    // sequencing, rate limiting, discovery, ISO-TP and the response path.
    static constexpr uint8_t echoRequestMessageTypeId = 0x01;

    // Accepts nothing shorter than echoMinimumValidatedPayloadSize, so that a
    // handler rejection and its invalidPayload acknowledgement can be observed.
    static constexpr uint8_t echoValidatedRequestMessageTypeId = 0x02;

    // Carries the echoed payload back to the requester.
    static constexpr uint8_t echoReplyMessageTypeId = 0x81;

    static constexpr std::size_t echoMinimumValidatedPayloadSize = 4;
}
