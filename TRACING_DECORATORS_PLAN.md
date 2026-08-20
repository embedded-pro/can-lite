# Implementation Plan: `can-lite` tracing decorators

**Status:** ready to implement
**Branch:** `claude/can-lite-instrumentation-tracing-j0uiuo`
**Scope:** one new module (`can-lite/tracing/`) holding a family of tracing decorators, one per
layer that exposes a usable seam. Phased — Phase 1 is self-contained and lands first.

> This file is a working handoff document, committed on the feature branch so a fresh session can pick
> it up. Delete it as part of the final commit that lands the implementation — it is scaffolding, not
> project documentation.

---

## 1. Goal

Instrument can-lite with tracing decorators that a consumer opts into by wiring them in the
composition root. Nothing in the library depends on a tracer; a build that does not construct a
decorator pays nothing.

## 2. Which layers can carry a tracing class

Assessed against the actual seams in the code, not by analogy.

| Layer | Seam | Decoratable | Decision |
|---|---|---|---|
| HAL | `hal::Can` — interface, 2 virtuals, taken by `CanProtocolServer`/`Client` ctors | yes | **`TracingCan`** — Phase 1 |
| Transport | `IsoTpTransport` — pure interface, 6 virtuals, attached via `AttachIsoTpTransport(IsoTpTransport&)` | yes | **`TracingIsoTpTransport`** — Phase 2 |
| Protocol | `CanProtocolServerObserver` / `CanProtocolClientObserver` | only after a small core change | **`TracingCanProtocolServerObserver` / `…ClientObserver`** — Phase 3 |
| Category | `CanCategory::HandleMessage` / `HandlePduMessage` | no | non-goal, see §9 |
| Core | `CanFrameTransport` — concrete class, no virtuals | no | covered by `TracingCan` |

Why the protocol layer is worth the extra step: the events it emits are the ones that produce **no bus
traffic at all**, so `TracingCan` structurally cannot show them — `OnCommandAckTimeout` and both
liveness `Offline` transitions are timer-driven (`CanProtocolClient` server-liveness timer,
`CanProtocolServer::clientLivenessTimer`). Without a protocol-layer tracer, a silent bus and a bus
whose acks are being missed look identical in the log.

## 3. Trace line convention (applies to every class in this module)

Every trace line starts with the emitting class's own name and a colon:

```
<ClassName>: <message>
```

Implement it as a file-local constant in each `.cpp`'s anonymous namespace and stream it first:

```cpp
namespace
{
    constexpr const char* tracePrefix = "TracingCan: ";
}
...
tracer.Trace() << tracePrefix << "TX id 0x" << infra::hex << rawId << ...;
```

This makes every line attributable to a layer and lets a mixed log be filtered with a single
`grep Tracing`.

### Naming

The family is named `Tracing<Thing>`, matching EMIL (`TracingFlash`, `TracingConnectionMbedTls`,
`TracingInputStream`, `TracingReset`) and this repo's own lint message in
`.github/linters/goodcheck.yml` (`amp.no-global-tracer`: *"Write a tracing decorator class instead"*).

**Note:** this renames the class previously sketched as `CanBusMonitor` to `TracingCan`, for
consistency now that it is one of several. Everything else about it is unchanged. If `CanBusMonitor`
is preferred, it is a rename of the class, its two files, and the `tracePrefix` constant — nothing
structural depends on it.

## 4. Why decorators rather than a `services::Tracer&` member per class

Threading a `Tracer&` through `CanFrameTransport`, the protocol classes and every category would
change every constructor signature, add a dependency to code that currently links `can_lite.core`
only, and leave dead weight in production builds. The repo has already ruled on this — see the
goodcheck rule above. EMIL's `services::TracingFlash` (`services/tracer/TracingFlash.hpp/.cpp`) is the
structural model to copy: a decorator over a `hal::` interface holding the delegate reference, a
`services::Tracer&`, and an `infra::AutoResetFunction` to wrap a completion callback. It is a closer
model than `TracingConnectionMbedTls.hpp`, which is the same idea buried under allocator and factory
plumbing we do not need.

## 5. Cross-cutting implementation rules

These apply to all three phases. Getting them wrong produces code that compiles in one build
configuration and not another.

1. `services::Tracer::Trace()` returns **different types** depending on the build:
   `infra::TextOutputStream` under `EMIL_ENABLE_TRACING`, and a dummy `Tracer::EmptyTracing` under
   `EMIL_DISABLE_TRACING`. So **never** write `auto stream = tracer.Trace();` or store the result.
   Each trace line must be one chained expression `tracer.Trace() << ... ;`.
2. **Never** use `tracer.Continue()` to append to a line `Trace()` started — `Continue()` is
   unconditional and would emit orphan text in a tracing-disabled build. Conditional extra
   information gets its own complete `Trace()` line.
3. Everything streamed must be **copyable by value** (`EmptyTracing::operator<<` takes `T` by value).
   `infra::hex`, `infra::AsHex(...)`, `const char*` and integers are all fine.
4. `infra::hex` is **sticky for the rest of the chain** and there is no `infra::dec` to switch back.
   Switch to hex once, early, and keep every numeric field after it in hex. Do not use `infra::Width`
   in a chain — it is sticky too and would pad unrelated fields. `infra::AsHex` is safe: it applies
   `hex`/`Width(2,'0')` to an internal copy of the stream.
5. `infra::AsHex` needs `infra::ConstByteRange`: use `infra::AsHex(infra::MakeRange(data))` for a
   `hal::Can::Message`; pass an `infra::ConstByteRange` straight through.
6. EMIL prints hex **lowercase and unpadded** (`OutputAsHexadecimal`, `hexChars = "0123456789abcdef"`),
   except inside `AsHex`, which pads every byte to two digits. Test expectations must match exactly.
7. **Every trace line is prefixed by `"\r\n"`** in the captured output — `Tracer::StartTrace()` emits it
   before the payload (EMIL's own `TestTracer.cpp`: `tracer.Trace() << "Text"` yields `"\r\nText"`).
8. House rules: **no comments in any new source file** (CLAUDE.md is explicit, including for
   non-obvious lines — rationale goes in the commit message); Allman braces, 4-space indent, `{}`
   initialisation, `PascalCase` methods, `camelCase` members, namespace `services`; no heap, no
   blocking, no `std::` containers; `#pragma once` (this repo's convention, not EMIL's include
   guards); warnings are errors.
9. Only `testing::StrictMock` in tests. Reuse `hal::CanMock` from `can_lite.test_util`; define
   per-file mocks in an anonymous namespace for the other interfaces, matching how
   `TestIsoTpChannel.cpp` and `TestCanProtocolServer.cpp` already do it.

---

## Phase 1 — `TracingCan` (HAL layer)

Wiring:

```cpp
services::TracingCan tracingCan{ realCan, tracer };
services::CanProtocolServer server{ tracingCan, config };   // instead of realCan
// or
services::CanProtocolClient client{ tracingCan };
```

`hal::Can` is the seam that carries **both** directions and sees **all** traffic, including frames
dropped later by node-ID filtering, rate limiting, or unregistered categories — which is exactly the
traffic you need when debugging.

### Files

`can-lite/tracing/TracingCan.hpp`, `TracingCan.cpp`, `can-lite/tracing/CMakeLists.txt`,
`can-lite/tracing/test/CMakeLists.txt`, `can-lite/tracing/test/TestTracingCan.cpp`, plus
`add_subdirectory(tracing)` at the end of `can-lite/CMakeLists.txt`.

### Header

```cpp
#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>

namespace services
{
    class TracingCan
        : public hal::Can
    {
    public:
        TracingCan(hal::Can& can, Tracer& tracer);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

    private:
        void TraceFrame(const char* direction, Id id, const Message& data);
        void TraceExtendedFrame(const char* direction, uint32_t rawId, const Message& data);
        void TraceCommandAck(const Message& data);

        hal::Can& can;
        Tracer& tracer;

        infra::Function<void(Id id, const Message& data)> receivedAction;
        infra::AutoResetFunction<void(bool success)> onSendDone;
    };
}
```

`hal::Can`'s destructor is `protected` and non-virtual and its copy operations are deleted, so no
destructor is needed here (CLAUDE.md: no pure-virtual destructors unless deleted polymorphically).

### Methods

```cpp
void TracingCan::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
{
    TraceFrame("TX", id, data);

    really_assert(!onSendDone);
    onSendDone = actionOnCompletion;

    can.SendData(id, data, [this](bool success)
        {
            if (!success)
                tracer.Trace() << tracePrefix << "TX failed";
            onSendDone(success);
        });
}

void TracingCan::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
{
    this->receivedAction = receivedAction;

    can.ReceiveData([this](Id id, const Message& data)
        {
            TraceFrame("RX", id, data);
            this->receivedAction(id, data);
        });
}
```

`#include "infra/util/ReallyAssert.hpp"` for `really_assert`.

**Why the single completion slot is safe, and why the assert is correct:** `CanFrameTransport`
serialises transmission — it keeps `sendInProgress` and queues everything else, so at most one
`SendData` is outstanding (`can-lite/core/CanFrameTransport.cpp:46`). The re-entrant case *is*
exercised: `CanFrameTransport`'s completion lambda calls `SendNextQueued()`, which issues the next
`can.SendData(...)` **before** invoking the caller's `done(success)`. That still works because
`infra::AutoResetFunction::operator()` clears the member (via `infra::PostAssign`) *before* invoking
the copy — so on re-entry `onSendDone` is empty and the assert holds. Do **not** substitute a plain
`infra::Function`; that breaks it. The assert follows the precedent set by
`CanFrameTransport::SetOnSendNotification` (`CanFrameTransport.cpp:26`).

### Name helpers

Anonymous namespace in the `.cpp` — a debug-only concern that must not grow `can_lite.core`'s public
API. Covered by the tests through observable output.

```cpp
const char* PriorityName(services::CanPriority priority);   // emergency|command|response|telemetry|heartbeat|unknown
const char* CategoryName(uint8_t category);                 // system|firmwareUpgrade|application
const char* MessageTypeName(uint8_t category, uint8_t messageType);
```

- `CategoryName`: `canSystemCategoryId` → `"system"`, `canFirmwareUpgradeCategoryId` →
  `"firmwareUpgrade"`, otherwise `"application"`.
- `MessageTypeName`: `canCategoryErrorResponseMessageTypeId` (0xFE) → `"categoryError"` for **any**
  category; then for `canSystemCategoryId`: 0x01 `"heartbeat"`, 0x02 `"commandAck"`, 0x03
  `"statusRequest"`, 0x04 `"categoryListRequest"`, 0x05 `"categoryListResponse"`; otherwise
  `"unknown"`.

Use the `constexpr` IDs from `can-lite/core/CanProtocolDefinitions.hpp`, never literals, and reuse the
existing `CanAckStatusToString` for the ack line. Every switch must be total with a fallback return —
a `switch` over `CanPriority` needs `return "unknown";` after it, since the value comes off the wire.

### Output format

```
TracingCan: TX id 0x4001123 prio command cat 0x0 system type 0x1 heartbeat node 0x123 dlc 3 data 010203
TracingCan: RX standard id 0x123 dlc 2 data 0102
TracingCan: TX failed
TracingCan: ack cat 0x3 type 0x5 status sequenceError expectedSeq 0x7
```

The `standard id` form covers 11-bit frames. This protocol never sends them (REQ-CAN-002) and the
stack silently discards them — showing them is precisely the point. `hal::Can::Id::Get29BitId()`
**asserts** on an 11-bit id and vice versa, so always branch on `Is11BitId()` first.

The `ack` line is an **additional line** after the frame line, emitted when
`category == canSystemCategoryId && messageType == canCommandAckMessageTypeId && data.size() >= canCommandAckSize`.
Payload layout is `[category, commandType, status, expectedSequence]` with **no** leading sequence
byte — see `CanProtocolServer::SendCommandAck` (`can-lite/server/CanProtocolServer.cpp:205`) and
`CanProtocolClient::HandleCommandAckFrame` (`can-lite/client/CanProtocolClient.cpp:195`).

Reference chains — keep the field order exactly:

```cpp
void TracingCan::TraceFrame(const char* direction, Id id, const Message& data)
{
    if (id.Is29BitId())
        TraceExtendedFrame(direction, id.Get29BitId(), data);
    else
        tracer.Trace() << tracePrefix << direction << infra::hex << " standard id 0x" << id.Get11BitId()
                       << " dlc " << data.size() << " data " << infra::AsHex(infra::MakeRange(data));
}

void TracingCan::TraceExtendedFrame(const char* direction, uint32_t rawId, const Message& data)
{
    auto category = ExtractCanCategory(rawId);
    auto messageType = ExtractCanMessageType(rawId);

    tracer.Trace() << tracePrefix << direction << infra::hex << " id 0x" << rawId
                   << " prio " << PriorityName(ExtractCanPriority(rawId))
                   << " cat 0x" << category << " " << CategoryName(category)
                   << " type 0x" << messageType << " " << MessageTypeName(category, messageType)
                   << " node 0x" << ExtractCanNodeId(rawId)
                   << " dlc " << data.size()
                   << " data " << infra::AsHex(infra::MakeRange(data));

    if (category == canSystemCategoryId && messageType == canCommandAckMessageTypeId && data.size() >= canCommandAckSize)
        TraceCommandAck(data);
}
```

DLC is 0–8, identical in hex or decimal, so rule §5.4 costs nothing here.

### Tests — `TestTracingCan.cpp`

Harness: `infra::StringOutputStream::WithStorage<512> stream; services::TracerToStream tracer{ stream };
StrictMock<hal::CanMock> can; TracingCan tracing{ can, tracer };`. The constructor must not call
anything on `can`, so a `StrictMock` with no expectations is valid at construction. Copy the
`MakeMessage` helper from `TestCanFrameTransport.cpp`.

| # | Test | Assertion |
|---|---|---|
| 1 | `SendDataIsForwardedToDelegate` | same id and payload reach the mock |
| 2 | `SendDataTracesDecodedExtendedFrame` | exact string for `MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, 0x123)` |
| 3 | `SendDataTracesStandardFrame` | 11-bit id → `"\r\nTracingCan: TX standard id 0x123 dlc 2 data 0102"`, no assert |
| 4 | `SuccessfulCompletionIsForwardedWithoutFailureTrace` | completion gets `true`, no `TX failed` |
| 5 | `FailedCompletionIsTracedAndForwarded` | output contains `"\r\nTracingCan: TX failed"`, completion gets `false` |
| 6 | `SecondSendReusesCompletionSlotAfterCompletion` | send → complete → send again does not trip the assert |
| 7 | `ReceiveDataRegistersWithDelegate` | `EXPECT_CALL(can, ReceiveData(_))` with `SaveArg<0>` |
| 8 | `ReceivedFrameIsTracedAndForwarded` | RX line **and** the registered action called with the same id/data |
| 9 | `CommandAckFrameIsDecodedOnItsOwnLine` | payload `{0x03, 0x05, 0x04, 0x07}` → frame line then `"\r\nTracingCan: ack cat 0x3 type 0x5 status sequenceError expectedSeq 0x7"` |
| 10 | `TruncatedCommandAckIsNotDecoded` | 3-byte payload → frame line only |
| 11 | `ApplicationCategoryAndUnknownMessageTypeAreNamedGenerically` | `... cat 0x3 application type 0x42 unknown ...` |
| 12 | `CategoryErrorMessageTypeIsNamedForAnyCategory` | type `0xfe` on an application category → `... type 0xfe categoryError ...` |
| 13 | `EmptyPayloadTracesZeroLengthData` | `dlc 0 data ` (trailing space, nothing after) |

Write expected strings out by hand — do not recompute the format in the test with the same helpers the
implementation uses, or the test proves nothing.

---

## Phase 2 — `TracingIsoTpTransport` (transport layer)

Pure addition; no change to existing code. Wiring:

```cpp
services::IsoTpTransportImpl::WithStorage<64, 4> isoTp{ canFrameTransport };
services::TracingIsoTpTransport tracingIsoTp{ isoTp, tracer };
server.AttachIsoTpTransport(tracingIsoTp);
```

This is the highest-value target after Phase 1: the ISO-TP sender/receiver FSMs are the hardest part
of the stack to debug from a frame log alone, and `AbortReason` is not on the wire at all.

### Header

```cpp
class TracingIsoTpTransport
    : public IsoTpTransport
{
public:
    TracingIsoTpTransport(IsoTpTransport& transport, Tracer& tracer);

    bool RegisterReceiveChannel(uint32_t dataId, uint32_t fcId) override;
    void ReleaseChannel(uint32_t dataId) override;
    bool SendPdu(uint32_t dataId, uint32_t fcId, infra::ConstByteRange pdu, const infra::Function<void()>& onDone) override;
    bool ProcessFrame(uint32_t canId, const hal::Can::Message& frame) override;
    void SetOnPduReceived(infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> callback) override;
    void SetOnAbort(infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> callback) override;

private:
    IsoTpTransport& transport;
    Tracer& tracer;
    infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> onPduReceived;
    infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> onAbort;
};
```

### Behaviour

- `RegisterReceiveChannel` / `ReleaseChannel` / `SendPdu`: trace, forward, return the delegate's result.
  Trace the **result** of the two `bool` calls — a `false` from `RegisterReceiveChannel` (channel pool
  exhausted) or `SendPdu` is a failure mode with no other visible symptom.
- **Do not wrap `SendPdu`'s `onDone`.** Unlike `hal::Can`, several channels can have sends in flight
  concurrently (`WithStorage<MaxPduSize, MaxChannels>`), so a single `AutoResetFunction` slot would be
  wrong here. Forward `onDone` unchanged.
- `ProcessFrame`: forward first, then **trace only when the delegate claimed the frame**.
  `CanProtocolServer::ProcessReceivedMessage` offers *every* received frame to the transport
  (`CanProtocolServer.cpp:141`), so tracing unconditionally would duplicate every RX line from
  Phase 1. A claim is the interesting event — it is the reason the frame never reached category
  dispatch.
- `SetOnPduReceived` / `SetOnAbort`: store the caller's callback, register a lambda that traces and
  then forwards — the same shape as `TracingCan::ReceiveData`.

Add an `AbortReasonName` helper in the anonymous namespace covering all five values of
`services::iso_tp::AbortReason` (`nBsTimeout`, `nCrTimeout`, `overflow`, `unexpectedFrame`,
`waitLimitExceeded`) plus a fallback return.

### Output format

```
TracingIsoTpTransport: RegisterReceiveChannel dataId 0x18db33f1 fcId 0x18da33f1 accepted
TracingIsoTpTransport: ReleaseChannel dataId 0x18db33f1
TracingIsoTpTransport: SendPdu dataId 0x18db33f1 fcId 0x18da33f1 size 0x40 accepted
TracingIsoTpTransport: ProcessFrame canId 0x18da33f1 claimed
TracingIsoTpTransport: PduReceived dataId 0x18db33f1 size 0x40 data 0102...
TracingIsoTpTransport: Abort dataId 0x18db33f1 reason nCrTimeout
```

Use `rejected` in place of `accepted` on a `false` result. A full PDU can be far larger than 8 bytes;
`infra::AsHex` on the whole range can overrun a small tracer buffer, so `size` comes first and the
`data` field is last on the line, where truncation is harmless.

### Tests — `TestTracingIsoTpTransport.cpp`

Define a `StrictMock` `IsoTpTransport` mock in the test file's anonymous namespace (there is no shared
one; `TestIsoTpChannel.cpp` and `TestCanProtocolServer.cpp` set the precedent for local mocks). Cover:
forwarding and return value of each method; `accepted`/`rejected` on both `bool` calls; `ProcessFrame`
traced only when claimed and **silent** when not; `PduReceived` and `Abort` traced and forwarded; each
`AbortReason` named.

---

## Phase 3 — protocol-layer tracing observers

**This phase requires a small change to core** and should be a separate commit with its own
justification. Do not fold it into Phase 1 or 2.

### The blocker and the change

`CanProtocolServerObserver` and `CanProtocolClientObserver` derive from
`infra::SingleObserver<...>`, which selects `infra::Subject`'s single-observer specialisation — a
second observer cannot attach. So a tracing observer would displace the application's.

Change both to `infra::Observer<...>`:

```cpp
class CanProtocolClientObserver
    : public infra::Observer<CanProtocolClientObserver, CanProtocolClient>
{
public:
    using infra::Observer<CanProtocolClientObserver, CanProtocolClient>::Observer;
    ...
};
```

`infra::Subject<T>` dispatches on `T::SingleHelper` — `SingleObserver` sets it to the subject type,
`Observer` sets it to `void` — so this one-line base-class swap flips the subject to the
multi-observer specialisation with no other edits. `NotifyObservers` and the public
`Subject()`/`Attached()`/`Attach()`/`Detach()` API are identical between the two. Cost is an
`IntrusiveList` in the subject and a list node per observer instead of a single pointer.

Apply this to the **two protocol observers only**. Category observers stay `SingleObserver` — the 1:1
category-to-consumer relationship documented in `architecture.md` §5 is genuine there. The protocol
subject is different: the application and a tracer are both legitimately interested.

`architecture.md` §5 currently states "Each subject supports exactly one observer". Amend it to record
the distinction and why.

### Classes

```cpp
class TracingCanProtocolServerObserver
    : public CanProtocolServerObserver
{
public:
    TracingCanProtocolServerObserver(CanProtocolServer& subject, Tracer& tracer);

    void Online() override;
    void Offline() override;

private:
    Tracer& tracer;
};
```

and the client equivalent overriding `OnServerOnline(uint16_t)`, `OnServerOffline(uint16_t)`,
`OnCommandAckTimeout(uint16_t, uint8_t, uint8_t)`.

### Output format

```
TracingCanProtocolServerObserver: Online
TracingCanProtocolServerObserver: Offline
TracingCanProtocolClientObserver: ServerOnline node 0x123
TracingCanProtocolClientObserver: ServerOffline node 0x123
TracingCanProtocolClientObserver: CommandAckTimeout node 0x123 cat 0x3 type 0x5
```

### Tests

Construct a real `CanProtocolServer`/`CanProtocolClient` over `StrictMock<hal::CanMock>` on an
`infra::ClockFixture`, attach both a `StrictMock` application observer **and** the tracing observer,
and assert that both fire — that is the regression test for the multi-observer change, not just for
the trace text. Use `ForwardTime()` to drive the liveness and ack timeouts.

---

## 6. CMake

Three targets so a consumer links only the layer it instruments, mirroring how the repo already splits
`can_lite.iso_tp` from `can_lite.transport`. All three live in `can-lite/tracing/CMakeLists.txt`,
modelled on `can-lite/transport/CMakeLists.txt`:

| Target | Sources | `target_link_libraries` |
|---|---|---|
| `can_lite.tracing` | `TracingCan.*` | `can_lite.core`, `services.tracer` |
| `can_lite.tracing_iso_tp` | `TracingIsoTpTransport.*` | `can_lite.transport`, `services.tracer` |
| `can_lite.tracing_protocol` | `TracingCanProtocol*Observer.*` | `can_lite.server`, `can_lite.client`, `services.tracer` |

Each gets the standard `target_include_directories` block (`$<BUILD_INTERFACE:...>/../..` and
`$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>`) and its own `emil_install(...)`. Add the targets as
their phases land — do not create empty targets in Phase 1.

`can_lite.core` already brings `hal.interfaces` and `infra.util` transitively (PUBLIC).
`services.tracer` is unconditionally defined by EMIL (`services/CMakeLists.txt` adds `tracer` first,
ungated by `EMIL_INCLUDE_ECHO`), so it is available despite this repo's `EMIL_INCLUDE_ECHO=Off` /
`EMIL_INCLUDE_MBEDTLS=Off`; it brings `infra.stream`.

`can-lite/CMakeLists.txt`: append `add_subdirectory(tracing)` **last**, so `can_lite.test_util` (defined
in `can-lite/core/test/CMakeLists.txt`) already exists.

`can-lite/tracing/test/CMakeLists.txt` follows `can-lite/core/test/CMakeLists.txt`: one
`can_lite.tracing_test` executable with `emil_build_for(... HOST All BOOL CAN_LITE_BUILD_TESTS)` and
`emil_add_test(...)`, linking the tracing targets plus `can_lite.test_util` and `gtest_main`.

Root `CMakeLists.txt` already installs `can-lite/**/*.hpp` with `test` excluded — nothing to add.

## 7. Documentation

Per CLAUDE.md's Document Consistency table. Phases 1 and 2 change no wire format or protocol
behaviour, so **do not** touch `documents/spec/can-protocol.md` or
`documents/requirements/can-protocol.yaml` — this is a debug facility, not protocol.

- `documents/design/architecture.md`: add `tracing/` to the §10 directory-structure block; add a §13
  "Observability" section with the layer table from §2, the wiring snippets, the line-prefix
  convention, and the target/layering rules. Phase 3 additionally amends §5's single-observer claim.
  Update `Last updated`.
- `README.md`: a bullet under **Features** and a `tracing/` line under **Project Structure**.

## 8. Definition of done (per phase)

```bash
cmake --preset host
cmake --build --preset host-Debug
ctest --preset host
```

- The phase's tests pass; the rest of the suite is unaffected.
- No warnings (they are errors).
- `grep -rn "GlobalTracer" can-lite/` returns nothing (goodcheck `amp.no-global-tracer`).
- No comments in any new source file; clang-format clean.

Note on tracing-disabled builds: with `EMIL_DISABLE_TRACING`, `Trace()` produces no output and the
string assertions would fail. `EMIL_ENABLE_TRACING` defaults to `On` in EMIL's root `CMakeLists.txt`
and this repo does not override it, so the `host` presets are fine. Do not add a CMake override; just
be aware if someone later turns it off.

Commit each phase separately on `claude/can-lite-instrumentation-tracing-j0uiuo` and push with
`git push -u origin claude/can-lite-instrumentation-tracing-j0uiuo`. Put the reasoning from §5 and the
per-phase notes in the commit bodies — that is where it lives, since the code carries no comments. Do
not open a PR unless asked.

## 9. Non-goals

- **A category-layer decorator.** `CanCategory::HandleMessage` and `HandlePduMessage` are
  **non-virtual** (`can-lite/core/CanCategory.hpp:40-41`); only `Id()` and
  `RequiresSequenceValidation()` are virtual, and `CanCategoryServer`'s constructor is protected and
  requires a `CanFrameTransport&`. Decorating a category would mean making dispatch virtual in core —
  a real design change adding vtable entries — for information that is nearly all derivable from the
  Phase 1 frame trace plus the decoded `commandAck` line (an unhandled message type already surfaces
  as an `unknownCommand` ack). Not worth it.
- **Tracing frames dropped inside `CanProtocolServer`** by the node-ID filter or the rate limiter
  (`CanProtocolServer.cpp:147,152`). These are invisible to every seam above — they would need an
  explicit hook in core. A rate-limit drop is a plausible real-world mystery, so this is the strongest
  candidate for a follow-up beyond Phase 3.
- **Frame counters / statistics** on `TracingCan` (frames sent, received, send failures).
- **A `CanFrameAnnotator` extension point** letting application categories register readable names for
  their own message types, replacing the generic `"application"` / `"unknown"` labels. Shape would be
  an `infra::IntrusiveList<CanFrameAnnotator>` on `TracingCan`, matching the category-registration
  idiom in `CanProtocolServer`.
- **Wiring the decorators into `ApplicationFixture`** behind an opt-in flag, so a failing BDD scenario
  can dump the bus.
