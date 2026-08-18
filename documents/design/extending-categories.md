# Extending can-lite with Categories

**Status:** Living document

can-lite ships two categories — System (`0x0`) and Firmware Upgrade (`0x1`).
Everything domain-specific belongs in the consuming project as an
**application category** using identifiers `0x2`–`0xF`. This document is the
reference for writing one.

A complete worked example lives in [`examples/foc_motor/`](../../examples/foc_motor/);
a minimal one used by the integration tests lives in
[`integration_tests/support/TestCategories.hpp`](../../integration_tests/support/TestCategories.hpp).

## 1. What a category is

A category is a **server/client pair** that shares one 4-bit category ID:

| Side   | Base class          | Owns                                        | Message type range |
|--------|---------------------|---------------------------------------------|--------------------|
| Server | `CanCategoryServer` | Command handlers, `Send*Response()` methods | `0x00`–`0x7F`      |
| Client | `CanCategoryClient` | Response handlers, `Send*Command()` methods | `0x80`–`0xFF`      |

The two base classes carry distinct `infra::IntrusiveList` node types, so the
compiler prevents registering a client category on a server and vice versa.

Only `can_lite.core` is needed to write a category — do not link
`can_lite.server` or `can_lite.client`.

## 2. Define the identifiers

Put every wire constant in one `*Definitions.hpp` so the wire format is
reviewable in a single place.

```cpp
#pragma once

#include <cstdint>

namespace services
{
    static constexpr uint8_t myCategoryId = 0x03;

    // Commands (client -> server), 0x00-0x7F
    static constexpr uint8_t mySetParametersId = 0x01;
    static constexpr uint8_t myQueryValueId = 0x02;

    // Responses (server -> client), 0x80-0xFF
    static constexpr uint8_t myValueResponseId = 0x82;

    enum class MyError : uint8_t
    {
        busy = 0,
        notSupported = 1
    };
}
```

`IsApplicationCategoryId()`, `IsCommandMessageType()` and
`IsResponseMessageType()` from `CanProtocolDefinitions.hpp` are `constexpr`, so
these can be asserted at compile time:

```cpp
static_assert(IsApplicationCategoryId(myCategoryId));
static_assert(IsCommandMessageType(mySetParametersId));
static_assert(IsResponseMessageType(myValueResponseId));
```

## 3. Bind handlers with `CanMessageHandler`

`CanMessageHandler<Owner>` binds a message type ID to a member function. It
stores an ID, a reference and a member pointer — no allocation, no vtable per
message, and no nested class per message type.

```cpp
class MyCategoryServer
    : public CanCategoryServer
    , public infra::Subject<MyCategoryServerObserver>
{
public:
    explicit MyCategoryServer(CanFrameTransport& transport);

    uint8_t Id() const override;

private:
    void HandleSetParameters(const hal::Can::Message& data);
    void HandleQueryValue(const hal::Can::Message& data);

    CanMessageHandler<MyCategoryServer> setParameters{ mySetParametersId, *this, &MyCategoryServer::HandleSetParameters };
    CanMessageHandler<MyCategoryServer> queryValue{ myQueryValueId, *this, &MyCategoryServer::HandleQueryValue };
};
```

Register them once in the constructor:

```cpp
MyCategoryServer::MyCategoryServer(CanFrameTransport& transport)
    : CanCategoryServer(transport)
{
    AddMessageTypes(setParameters, queryValue);
}
```

For a message that also arrives as a reassembled ISO-TP PDU, pass a second
member function: `CanMessageHandler<Owner>{ id, *this, &Owner::HandleFrame, &Owner::HandlePdu }`.

## 4. Read and write payloads with `CanPayload`

`CanPayloadReader` and `CanPayloadWriter` are cursor-based and big-endian, so no
byte offsets are computed by hand. Both track validity as a **sticky flag**: one
check covers the whole payload.

```cpp
void MyCategoryServer::HandleSetParameters(const hal::Can::Message& data)
{
    CanPayloadReader reader{ data };
    reader.Skip(1);                       // sequence byte
    auto first = reader.ReadInt16();
    auto second = reader.ReadInt16();

    if (!reader.Valid())
    {
        SendCommandAck(mySetParametersId, CanAckStatus::invalidPayload);
        return;
    }

    // ...
}
```

Reads past the end return zero and clear `Valid()`; writes past 8 bytes are
dropped and clear `Valid()`. `SendResponse()` and `SendCommand()` refuse to send
an invalid payload, so overflow can never reach the bus.

Available: `ReadUInt8`, `ReadInt16`, `ReadUInt16`, `ReadInt32`, `ReadUInt32`,
`ReadFixed16(scale)`, `Skip`, `ReadRemaining`, `Available` — and the matching
`Write*` methods plus `WriteBytes`.

## 5. Send frames

The base classes own the transport and fill in the category ID and priority.

**Server:**

| Method                                          | Priority    | Message type |
|-------------------------------------------------|-------------|--------------|
| `SendResponse(messageType, payload)`            | `response`  | as given     |
| `SendTelemetry(messageType, payload)`           | `telemetry` | as given     |
| `SendCategoryError(originatingCommandId, code)` | `response`  | `0xFE`       |
| `SendCommandAck(messageType, status)`           | `response`  | system ACK   |

**Client:**

| Method                                                                   | Sequence byte           |
|--------------------------------------------------------------------------|-------------------------|
| `SendCommand(nodeId, messageType[, payload][, priority])`                | prepended automatically |
| `SendCommandWithoutSequence(nodeId, messageType[, payload][, priority])` | none                    |

```cpp
bool MyCategoryClient::SendSetParameters(uint16_t targetNodeId, int16_t first, int16_t second)
{
    CanPayloadWriter payload;
    payload.WriteInt16(first).WriteInt16(second);

    return SendCommand(targetNodeId, mySetParametersId, payload);
}
```

Do **not** write the sequence byte yourself — `SendCommand()` prepends it and
only advances the counter once the frame is accepted by the send queue.

Use `SendCommandWithoutSequence()` only when the matching server category
overrides `RequiresSequenceValidation()` to return `false` (as Firmware Upgrade
does, because block indices already order the transfer). `priority` defaults to
`CanPriority::command`; pass `CanPriority::emergency` for safety-critical
commands.

## 6. Report errors

Two mechanisms, used together:

- **`CanAckStatus`** — the universal outcome every command reports
  (`success`, `invalidPayload`, `invalidState`, `rateLimited`, ...).
- **Category error response (`0xFE`)** — a category-defined error code when the
  universal status is not specific enough. Payload is
  `{ originatingCommandId, categoryErrorCode }`.

```cpp
SendCategoryError(myFailingCommandId, static_cast<uint8_t>(MyError::busy));
SendCommandAck(myFailingCommandId, CanAckStatus::categoryError);
```

Client categories observe it by binding a handler to
`canCategoryErrorResponseMessageTypeId`.

## 7. Expose events through an observer

Every category notifies its consumer through
`infra::Subject` / `infra::SingleObserver`. Asynchronous work is handed back
through an `infra::Function` completion callback so handlers never block.

```cpp
class MyCategoryServerObserver
    : public infra::SingleObserver<MyCategoryServerObserver, MyCategoryServer>
{
public:
    using infra::SingleObserver<MyCategoryServerObserver, MyCategoryServer>::SingleObserver;

    virtual void OnSetParameters(int16_t first, int16_t second, const infra::Function<void()>& onDone) = 0;
};
```

Observer callbacks must not allocate and must not block.

## 8. Register

```cpp
CanProtocolServer server{ can, config };
MyCategoryServer myCategory{ server.Transport() };
server.RegisterCategory(myCategory);

CanProtocolClient client{ can };
MyCategoryClient myCategoryClient{ client.Transport(), client };
client.RegisterCategory(myCategoryClient);
```

`CanProtocolClient` implements `CanSequenceSource`, which is all a client
category needs — categories never depend on `CanProtocolClient` itself.

Always use `client.Transport()` for a category's `CanFrameTransport&`, rather
than constructing a second, independent `CanFrameTransport` over the same
`hal::Can`. Each `CanFrameTransport` owns its own `sendInProgress` flag and
send queue; two independent transports wrapping the same `hal::Can` would
each believe they alone own the outstanding transmission, and concurrent
sends from both could overlap on the HAL.

Registering two categories with the same ID asserts at runtime. Registered
category IDs are reported automatically by category discovery.

## 9. Test

Unit tests use GoogleTest with `testing::StrictMock<>` only. `can_lite.test_util`
provides `hal::CanMock`, plus `CanCategoryServerStub` / `CanCategoryClientStub`,
which own a CAN mock, a `CanFrameTransport` and a `CanSequenceSource` so a test
category stays default-constructible.

Cover at minimum, per message type: the happy path, a short payload, and — for
server categories — an out-of-sequence command.
