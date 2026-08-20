# Authoring a Category

This chapter builds a category from nothing, using the **demo category** that
the integration tests ship with (`integration_tests/support/TestCategories.hpp`)
as the worked example. It is deliberately minimal but covers all four shapes a
real category needs: a fire-and-forget command, a command with a payload, a
query with a response, and a failure that carries a category-specific error
code.

Application categories belong in the **consuming project**, not in
`can-lite/categories/`. The library ships only protocol-level categories.

## 1. Pick identifiers

```cpp
#pragma once

#include <cstdint>

namespace myproduct
{
    static constexpr uint8_t demoCategoryId = 0x03;

    static constexpr uint8_t demoPingId = 0x00;
    static constexpr uint8_t demoSetParametersId = 0x01;
    static constexpr uint8_t demoQueryValueId = 0x02;
    static constexpr uint8_t demoFailId = 0x03;

    static constexpr uint8_t demoValueResponseId = 0x82;

    enum class DemoError : uint8_t
    {
        notSupported = 0,
        busy = 1
    };
}
```

| Rule | Why |
|------|-----|
| Category ID in `0x02`–`0x0F` | `0x0` and `0x1` are the built-in categories; `IsApplicationCategoryId()` encodes the range |
| Commands `0x00`–`0x7F` | The server drops response-range message types before dispatch |
| Responses `0x80`–`0xFF` | Convention: `0x80 + command ID` when there is a natural pairing |
| Never define `0xFE` | Reserved in every category for the category-error response |
| One header for all of it | The server and client halves must agree, and a single header makes disagreement impossible |

`0x82` for the value response follows the `0x80 + 0x02` convention against
`demoQueryValueId = 0x02`.

## 2. Design the payloads

Budget the bytes before writing code. A sequence-validated command has **seven**
usable bytes, because `data[0]` is the sequence number.

| Message | Layout | Bytes |
|---------|--------|-------|
| Ping `0x00` | sequence | 1 |
| Set Parameters `0x01` | sequence, `int16` first, `int16` second, `int16` third | 7 |
| Query Value `0x02` | sequence | 1 |
| Fail `0x03` | sequence | 1 |
| Value Response `0x82` | `int16` value | 2 |
| Category Error `0xFE` | originating command ID, error code | 2 |

If a message does not fit in seven bytes, the options are, in order of
preference: split it into several messages; scale a float into a smaller
fixed-point field (Chapter 5, §4); or attach ISO-TP and give the message type a
PDU handler (Chapter 9, §7).

## 3. The server half

```cpp
class DemoCategoryServerObserver
    : public infra::SingleObserver<DemoCategoryServerObserver, DemoCategoryServer>
{
public:
    using infra::SingleObserver<DemoCategoryServerObserver, DemoCategoryServer>::SingleObserver;

    virtual void OnPing(const infra::Function<void()>& onDone) = 0;
    virtual void OnSetParameters(const DemoParameters& parameters, const infra::Function<void()>& onDone) = 0;
    virtual void OnQueryValue(const infra::Function<void(int16_t)>& onResult) = 0;
    virtual void OnFail(const infra::Function<void(DemoError)>& onResult) = 0;
};

class DemoCategoryServer
    : public services::CanCategoryServer
    , public infra::Subject<DemoCategoryServerObserver>
{
public:
    explicit DemoCategoryServer(services::CanFrameTransport& transport);

    uint8_t Id() const override;

private:
    void HandlePing(const hal::Can::Message& data);
    void HandleSetParameters(const hal::Can::Message& data);
    void HandleQueryValue(const hal::Can::Message& data);
    void HandleFail(const hal::Can::Message& data);

    void SendValueResponse(int16_t value);

    services::CanMessageHandler<DemoCategoryServer> ping{ demoPingId, *this, &DemoCategoryServer::HandlePing };
    services::CanMessageHandler<DemoCategoryServer> setParameters{ demoSetParametersId, *this, &DemoCategoryServer::HandleSetParameters };
    services::CanMessageHandler<DemoCategoryServer> queryValue{ demoQueryValueId, *this, &DemoCategoryServer::HandleQueryValue };
    services::CanMessageHandler<DemoCategoryServer> fail{ demoFailId, *this, &DemoCategoryServer::HandleFail };
};
```

```cpp
DemoCategoryServer::DemoCategoryServer(services::CanFrameTransport& transport)
    : services::CanCategoryServer(transport)
{
    AddMessageTypes(ping, setParameters, queryValue, fail);
}

uint8_t DemoCategoryServer::Id() const
{
    return demoCategoryId;
}
```

`RequiresSequenceValidation()` is not overridden, so it keeps the server-side
default of `true`.

### Shape 1 — fire and forget

```cpp
void DemoCategoryServer::HandlePing(const hal::Can::Message&)
{
    NotifyObservers([this](auto& observer)
        {
            observer.OnPing([this]()
                {
                    SendCommandAck(demoPingId, services::CanAckStatus::success);
                });
        });
}
```

No payload to parse, no response frame: the acknowledgement *is* the answer. The
completion function still exists, so a handler that must do something slow can
acknowledge later.

### Shape 2 — a command with a payload

```cpp
void DemoCategoryServer::HandleSetParameters(const hal::Can::Message& data)
{
    services::CanPayloadReader reader{ data };
    reader.Skip(1);
    DemoParameters parameters{ reader.ReadInt16(), reader.ReadInt16(), reader.ReadInt16() };

    if (!reader.Valid())
    {
        SendCommandAck(demoSetParametersId, services::CanAckStatus::invalidPayload);
        return;
    }

    NotifyObservers([this, parameters](auto& observer)
        {
            observer.OnSetParameters(parameters, [this]()
                {
                    SendCommandAck(demoSetParametersId, services::CanAckStatus::success);
                });
        });
}
```

The three-line pattern to internalise: `Skip(1)` because this category validates
sequences, read every field, then check `Valid()` **once** before acting. Reads
past the end return zero rather than trapping, so skipping the check means
silently acting on zeros.

Note the parameters are captured **by value** into the notify lambda: `data` is
a reference to a frame owned by the receive path and must not outlive it.

### Shape 3 — query and response

```cpp
void DemoCategoryServer::HandleQueryValue(const hal::Can::Message&)
{
    NotifyObservers([this](auto& observer)
        {
            observer.OnQueryValue([this](int16_t value)
                {
                    SendValueResponse(value);
                    SendCommandAck(demoQueryValueId, services::CanAckStatus::success);
                });
        });
}

void DemoCategoryServer::SendValueResponse(int16_t value)
{
    services::CanPayloadWriter payload;
    payload.WriteInt16(value);

    SendResponse(demoValueResponseId, payload);
}
```

Response first, acknowledgement second. The order is a convention rather than a
requirement, but it is the one the built-in categories follow, and it means a
client that treats the acknowledgement as "the exchange is finished" has already
received the data.

### Shape 4 — a category-specific failure

```cpp
void DemoCategoryServer::HandleFail(const hal::Can::Message&)
{
    NotifyObservers([this](auto& observer)
        {
            observer.OnFail([this](DemoError error)
                {
                    SendCategoryError(demoFailId, static_cast<uint8_t>(error));
                    SendCommandAck(demoFailId, services::CanAckStatus::categoryError);
                });
        });
}
```

Use `SendCategoryError` when the failure has a category-specific meaning the
`CanAckStatus` enum cannot express, and pair it with a `categoryError`
acknowledgement so the client knows to look at the `0xFE` frame.

## 4. The client half

```cpp
class DemoCategoryClient
    : public services::CanCategoryClient
    , public infra::Subject<DemoCategoryClientObserver>
{
public:
    DemoCategoryClient(services::CanFrameTransport& transport,
        services::CanSequenceSource& sequenceSource);

    uint8_t Id() const override;

    bool SendPing(uint16_t targetNodeId);
    bool SendSetParameters(uint16_t targetNodeId, const DemoParameters& parameters);
    bool SendQueryValue(uint16_t targetNodeId);
    bool SendFail(uint16_t targetNodeId);

private:
    void HandleValueResponse(const hal::Can::Message& data);
    void HandleCategoryError(const hal::Can::Message& data);

    services::CanMessageHandler<DemoCategoryClient> valueResponse{
        demoValueResponseId, *this, &DemoCategoryClient::HandleValueResponse };
    services::CanMessageHandler<DemoCategoryClient> categoryError{
        services::canCategoryErrorResponseMessageTypeId, *this, &DemoCategoryClient::HandleCategoryError };
};
```

Sending is a one-liner per command, and every one of them takes the target node:

```cpp
bool DemoCategoryClient::SendSetParameters(uint16_t targetNodeId, const DemoParameters& parameters)
{
    services::CanPayloadWriter payload;
    payload.WriteInt16(parameters.first).WriteInt16(parameters.second).WriteInt16(parameters.third);

    return SendCommand(targetNodeId, demoSetParametersId, payload);
}
```

`SendCommand` prepends the sequence byte (Chapter 6, §4). Had the paired server
declared `RequiresSequenceValidation()` as `false`, every one of these would use
`SendCommandWithoutSequence` instead — and the server's handlers would not
`Skip(1)`.

Receiving mirrors the server, minus the acknowledgement:

```cpp
void DemoCategoryClient::HandleValueResponse(const hal::Can::Message& data)
{
    services::CanPayloadReader reader{ data };
    auto value = reader.ReadInt16();

    if (!reader.Valid())
        return;

    NotifyObservers([value](auto& observer) { observer.OnValueResponse(value); });
}
```

A malformed response is dropped, not acknowledged: clients do not acknowledge,
and there is nothing useful to send back.

Registering a handler for `canCategoryErrorResponseMessageTypeId` on the client
is what turns the server's `SendCategoryError` into an application event. Every
category that can fail should register one.

## 5. Wire it up

```cpp
// Server node
services::CanProtocolServer server{ can, config };
myproduct::DemoCategoryServer demoServer{ server.Transport() };
MyDemoHandler handler{ demoServer };            // observer attaches

if (!server.RegisterCategory(demoServer))
    ReportStartupFault();                       // duplicate ID or eight already registered

// Client node
services::CanProtocolClient client{ can };
myproduct::DemoCategoryClient demoClient{ client.Transport(), client };
MyDemoObserver observer{ demoClient };

if (!client.RegisterCategory(demoClient))
    ReportStartupFault();
```

Note the client category takes `client` twice, in two roles: as
`CanFrameTransport&` (via `client.Transport()`) and as `CanSequenceSource&`
(`client` itself implements it). That is the indirection that lets the category
link `can_lite.core` only.

**Check the return value of `RegisterCategory`.** An unregistered category is
indistinguishable, on the wire, from a category that does not exist: its
commands are dropped silently and its client waits for a response that never
comes.

## 6. CMake

```cmake
add_library(myproduct.can_demo)

target_link_libraries(myproduct.can_demo PUBLIC
    can_lite.core
)

target_sources(myproduct.can_demo PRIVATE
    DemoDefinitions.hpp
    DemoCategoryServer.cpp
    DemoCategoryServer.hpp
    DemoCategoryClient.cpp
    DemoCategoryClient.hpp
)
```

`can_lite.core` and nothing else. Linking `can_lite.server` or
`can_lite.client` from a category is the one build-level mistake that is worth
watching for in review: it compiles, it works, and it quietly makes the category
unusable on the other side of the bus.

## 7. Test it

Unit-test each half against a mocked `hal::Can`, with `StrictMock` observers —
`NiceMock` and bare mocks are not permitted in this codebase:

```cpp
TEST_F(DemoCategoryServerTest, set_parameters_notifies_observer_and_acknowledges)
{
    EXPECT_CALL(observer, OnSetParameters(FieldsAre(100, 200, 300), _))
        .WillOnce([](const DemoParameters&, const infra::Function<void()>& onDone) { onDone(); });
    EXPECT_CALL(can, SendData(AckFrame(demoCategoryId, demoSetParametersId, CanAckStatus::success), _));

    ReceiveCommand(demoSetParametersId, { 0x00, 0x00, 0x64, 0x00, 0xC8, 0x01, 0x2C });
}
```

Then exercise the pair end to end in an integration scenario, over the virtual
bus (Chapter 15):

```gherkin
Scenario: Setting parameters is acknowledged
    Given a server with the demo category registered
    When the client sends set parameters 100, 200, 300
    Then the server observer receives the parameters
    And the client receives a success acknowledgement
```

The tests worth writing for every category, beyond the happy path:

| Test | What it protects |
|------|------------------|
| Truncated payload | The `Valid()` check produces `invalidPayload`, not garbage |
| Unknown message type in the category | `unknownCommand` acknowledgement |
| Response with a truncated payload | The client drops it instead of notifying with zeros |
| Sequence gap | `sequenceError` with the expected value, and recovery afterwards |
| Category error path | The `0xFE` frame reaches the client observer |
| Send while the transport queue is full | `SendCommand` returns `false` and no sequence number is consumed |

## 8. Checklist

| Step | Done when |
|------|-----------|
| 1. Definitions header | Category ID and every message type ID are `constexpr`, in one header shared by both halves |
| 2. Server class | Derives `CanCategoryServer` + `infra::Subject`, registers command handlers in the constructor |
| 3. Client class | Derives `CanCategoryClient` + `infra::Subject`, registers response handlers, exposes one send method per command |
| 4. Sequence policy | Server's `RequiresSequenceValidation()` and the client's choice of `SendCommand` vs `SendCommandWithoutSequence` agree |
| 5. Payloads | Only `CanPayloadReader`/`CanPayloadWriter`; `Valid()` checked once per handler |
| 6. Errors | `CanAckStatus` for protocol-level, `SendCategoryError` + a `0xFE` client handler for category-level |
| 7. Observers | `infra::SingleObserver` interfaces; asynchronous callbacks take a completion `infra::Function` |
| 8. Build | Links `can_lite.core` only |
| 9. Registration | Both halves registered, return values checked |
| 10. Tests | Unit tests with `StrictMock` for both halves, plus an end-to-end scenario |
| 11. Documentation | A category specification alongside `documents/spec/`, listing every message and its payload layout |
