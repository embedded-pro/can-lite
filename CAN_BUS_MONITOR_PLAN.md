# Implementation Plan: `services::CanBusMonitor`

**Status:** ready to implement
**Branch:** `claude/can-lite-instrumentation-tracing-j0uiuo`
**Scope:** one new module (`can-lite/tracing/`), one new class, one new unit-test binary, doc updates.

> This file is a working handoff document, committed on the feature branch so a fresh session can pick
> it up. Delete it as part of the final commit that lands the implementation — it is scaffolding, not
> project documentation.

---

## 1. Goal

Add a protocol-aware, zero-cost-when-absent **CAN bus monitor**: a `hal::Can` decorator that traces
every frame crossing the bus in both directions, decoding the 29-bit identifier into human-readable
priority / category / message-type / node fields, plus a decoded `CommandAck` line.

Wiring is a one-line change in the consumer's composition root — no changes to `CanProtocolServer`,
`CanProtocolClient`, `CanFrameTransport`, or any category:

```cpp
services::CanBusMonitor monitoredCan{ realCan, tracer };
services::CanProtocolServer server{ monitoredCan, config };   // instead of realCan
// or
services::CanProtocolClient client{ monitoredCan };
```

## 2. Why this design (do not deviate without saying why)

- **`hal::Can` is the right seam.** It is the only real interface in the stack with both directions on
  it (`SendData` / `ReceiveData`), it is a two-method interface, and both `CanProtocolServer` and
  `CanProtocolClient` take `hal::Can&` in their constructors. Decorating it captures **all** traffic,
  including frames dropped later by node-ID filtering, rate limiting, or unregistered categories —
  which is exactly the traffic you need to see when debugging.
- **A decorator, not a `services::Tracer&` member threaded through every class.** Threading a tracer
  through `CanFrameTransport`/categories would change every constructor signature, add a dependency to
  classes that currently link `can_lite.core` only, and leave dead weight in production builds. The
  repo has already ruled on this: `.github/linters/goodcheck.yml` rule `amp.no-global-tracer` says
  *"Write a tracing decorator class instead."*
- **Reference implementation to mirror:** EMIL's `services::TracingFlash`
  (`services/tracer/TracingFlash.hpp/.cpp`) — a decorator over a `hal::` interface holding
  `hal::Flash& flash; services::Tracer& tracer;` and an `infra::AutoResetFunction` to wrap the
  completion callback. That is a near-exact structural match for this task and is a closer model than
  `TracingConnectionMbedTls.hpp` (which is the same idea applied to a class hierarchy with allocators
  and factories — we need none of that here).

## 3. Files to create / change

| File | Action |
|---|---|
| `can-lite/tracing/CanBusMonitor.hpp` | new |
| `can-lite/tracing/CanBusMonitor.cpp` | new |
| `can-lite/tracing/CMakeLists.txt` | new |
| `can-lite/tracing/test/CMakeLists.txt` | new |
| `can-lite/tracing/test/TestCanBusMonitor.cpp` | new |
| `can-lite/CMakeLists.txt` | add `add_subdirectory(tracing)` |
| `documents/design/architecture.md` | new section + directory-structure block |
| `README.md` | Features + Project Structure |

No changes to `can-lite/core/`, `server/`, `client/`, `categories/`, or `transport/`.

## 4. Header — `can-lite/tracing/CanBusMonitor.hpp`

```cpp
#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>

namespace services
{
    class CanBusMonitor
        : public hal::Can
    {
    public:
        CanBusMonitor(hal::Can& can, Tracer& tracer);

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

Notes:

- `hal::Can`'s destructor is `protected` and non-virtual, and its copy operations are deleted — so no
  destructor is needed here (CLAUDE.md: no pure-virtual destructors unless deleted polymorphically).
- `#pragma once` (matches every other header in this repo; EMIL uses include guards, this repo does not).

## 5. Implementation — `can-lite/tracing/CanBusMonitor.cpp`

### 5.1 Name helpers

Put them in an **anonymous namespace** in the `.cpp`. They are a debug-only concern and must not grow
`can_lite.core`'s public API. They are covered by the monitor's own tests through observable output.

```cpp
namespace
{
    const char* PriorityName(services::CanPriority priority);      // emergency|command|response|telemetry|heartbeat|unknown
    const char* CategoryName(uint8_t category);                    // system|firmwareUpgrade|application
    const char* MessageTypeName(uint8_t category, uint8_t messageType);
}
```

`CategoryName`: `canSystemCategoryId` → `"system"`, `canFirmwareUpgradeCategoryId` →
`"firmwareUpgrade"`, otherwise `"application"`.

`MessageTypeName`:
- `messageType == canCategoryErrorResponseMessageTypeId` (0xFE) → `"categoryError"` for **any** category.
- `category == canSystemCategoryId`: 0x01 `"heartbeat"`, 0x02 `"commandAck"`, 0x03 `"statusRequest"`,
  0x04 `"categoryListRequest"`, 0x05 `"categoryListResponse"`.
- everything else → `"unknown"`.

Use the `constexpr` IDs from `can-lite/core/CanProtocolDefinitions.hpp`, never literals. Reuse the
existing `CanAckStatusToString` from that header for the ack line.

Every switch must be total and return a fallback string — a `switch` over `CanPriority` needs a
`return "unknown";` after it so a non-enumerator bit pattern from the wire is handled.

### 5.2 Methods

```cpp
CanBusMonitor::CanBusMonitor(hal::Can& can, Tracer& tracer)
    : can(can)
    , tracer(tracer)
{}

void CanBusMonitor::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
{
    TraceFrame("TX", id, data);

    really_assert(!onSendDone);
    onSendDone = actionOnCompletion;

    can.SendData(id, data, [this](bool success)
        {
            if (!success)
                tracer.Trace() << "CAN TX failed";
            onSendDone(success);
        });
}

void CanBusMonitor::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
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
`SendData` is outstanding at a time (`can-lite/core/CanFrameTransport.cpp:46`). The re-entrant case
*is* exercised in practice: `CanFrameTransport`'s completion lambda calls `SendNextQueued()`, which
issues the next `can.SendData(...)` **before** invoking the caller's `done(success)`. That still works
here because `infra::AutoResetFunction::operator()` clears the member (via `infra::PostAssign`)
*before* invoking the copy — so by the time control re-enters `SendData`, `onSendDone` is empty and
the assert holds. Do not replace `AutoResetFunction` with a plain `infra::Function`; that would break
this. The assert follows the precedent already set by
`CanFrameTransport::SetOnSendNotification` (`CanFrameTransport.cpp:26`) for a single-slot invariant.

### 5.3 Trace formatting — the important gotchas

`services::Tracer::Trace()` returns **different types** depending on the build:
`infra::TextOutputStream` under `EMIL_ENABLE_TRACING`, and a dummy `Tracer::EmptyTracing` under
`EMIL_DISABLE_TRACING`. Therefore:

1. **Never** write `auto stream = tracer.Trace();` or store the result. Each trace line must be a
   single chained expression `tracer.Trace() << ... << ...;` so it also compiles when tracing is
   compiled out.
2. **Never** use `tracer.Continue()` to append to a line that `Trace()` started — `Continue()` is
   unconditional and would emit orphan text in a tracing-disabled build. Conditional extra
   information (the ack decode) gets its **own complete `Trace()` line**.
3. Everything streamed with `<<` must be **copyable by value** (`EmptyTracing::operator<<` takes `T`
   by value). `infra::hex`, `infra::AsHex(...)`, `const char*` and integers are all fine.
4. `infra::hex` is **sticky for the rest of the chain** and there is no `infra::dec` to switch back.
   So switch to hex once, early, and keep every numeric field after it in hex. This is fine: DLC is
   0–8, identical in either radix. Do not use `infra::Width` in the chain — it is sticky too and would
   pad unrelated fields. `infra::AsHex` is safe: it applies `hex`/`Width(2,'0')` to an internal copy of
   the stream, so it does not leak state.
5. `infra::AsHex` needs `infra::ConstByteRange`: use `infra::AsHex(infra::MakeRange(data))` —
   `MakeRange(const infra::BoundedVector<uint8_t>&)` yields `MemoryRange<const uint8_t>`.
   `#include "infra/stream/OutputStream.hpp"` is pulled in via `Tracer.hpp`.
6. EMIL prints hex **lowercase and unpadded** (`OutputAsHexadecimal`, `hexChars = "0123456789abcdef"`),
   except inside `AsHex`, which pads every byte to two digits. Test expectations must match exactly.
7. `hal::Can::Id::Get29BitId()` **asserts** when the id is an 11-bit id (and vice versa). Always branch
   on `Is11BitId()` first.

### 5.4 Output format

Extended (29-bit) frame — the normal case:

```
CAN TX id 0x4001123 prio command cat 0x0 system type 0x1 heartbeat node 0x123 dlc 3 data 010203
```

Standard (11-bit) frame — not used by this protocol (REQ-CAN-002), but the monitor must still show it,
since seeing the frame the stack silently discards is the whole point:

```
CAN RX standard id 0x123 dlc 2 data 0102
```

Failed transmission (own line, emitted from the completion callback):

```
CAN TX failed
```

Decoded acknowledgement — emitted as an **additional line** immediately after the frame line, when
`category == canSystemCategoryId && messageType == canCommandAckMessageTypeId && data.size() >= canCommandAckSize`:

```
CAN ack cat 0x3 type 0x5 status sequenceError expectedSeq 0x7
```

Ack payload layout is `[category, commandType, status, expectedSequence]` with **no** leading sequence
byte — see `CanProtocolServer::SendCommandAck` (`can-lite/server/CanProtocolServer.cpp:205`) and
`CanProtocolClient::HandleCommandAckFrame` (`can-lite/client/CanProtocolClient.cpp:195`).

Reference chains (keep the field order exactly):

```cpp
void CanBusMonitor::TraceFrame(const char* direction, Id id, const Message& data)
{
    if (id.Is29BitId())
        TraceExtendedFrame(direction, id.Get29BitId(), data);
    else
        tracer.Trace() << "CAN " << direction << infra::hex << " standard id 0x" << id.Get11BitId()
                       << " dlc " << data.size() << " data " << infra::AsHex(infra::MakeRange(data));
}

void CanBusMonitor::TraceExtendedFrame(const char* direction, uint32_t rawId, const Message& data)
{
    auto category = ExtractCanCategory(rawId);
    auto messageType = ExtractCanMessageType(rawId);

    tracer.Trace() << "CAN " << direction << infra::hex << " id 0x" << rawId
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

Watch the types: `PriorityName` etc. take the values returned by the `Extract*` helpers; `category`
and `messageType` are `uint8_t`, which `TextOutputStream` prints as a number (not a character).
`data.size()` is `std::size_t` and goes through the integral overload.

Keep every function ≤ 30 lines (CLAUDE.md hard limit 50). `TraceExtendedFrame` above is ~15.

### 5.5 House rules that apply here

- **No comments anywhere in the code** — CLAUDE.md is explicit, including for non-obvious lines. All the
  rationale in §5.2/§5.3 belongs in the commit message, not in the source.
- Allman braces, 4-space indent, `{}` initialisation, `PascalCase` methods, `camelCase` members,
  namespace `services`.
- No heap, no blocking, no `std::` containers.
- Warnings are errors — an unused parameter or a narrowing conversion will fail the build.

## 6. CMake

### `can-lite/tracing/CMakeLists.txt`

Model it on `can-lite/transport/CMakeLists.txt`:

```cmake
add_library(can_lite.tracing ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.tracing PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.tracing PUBLIC
    can_lite.core
    services.tracer
)

target_sources(can_lite.tracing PRIVATE
    CanBusMonitor.cpp
    CanBusMonitor.hpp
)

emil_install(can_lite.tracing
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_subdirectory(test)
```

`can_lite.core` already brings `hal.interfaces` and `infra.util` transitively (PUBLIC). `services.tracer`
is unconditionally defined by EMIL (`services/CMakeLists.txt` adds `tracer` first, ungated by
`EMIL_INCLUDE_ECHO`), so it is available even with this repo's `EMIL_INCLUDE_ECHO=Off` /
`EMIL_INCLUDE_MBEDTLS=Off` settings. It brings `infra.stream` with it.

`can_lite.tracing` links **core only** — never `can_lite.server` or `can_lite.client`.

### `can-lite/CMakeLists.txt`

Append at the end:

```cmake
add_subdirectory(tracing)
```

### `can-lite/tracing/test/CMakeLists.txt`

Model it on `can-lite/core/test/CMakeLists.txt` (the `can_lite.test_util` interface target there
already exports `CanMock.hpp`, so reuse it — do not write a second CAN mock):

```cmake
add_executable(can_lite.tracing_test)
emil_build_for(can_lite.tracing_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.tracing_test)

target_link_libraries(can_lite.tracing_test PRIVATE
    can_lite.tracing
    can_lite.test_util
    gtest_main
)

target_sources(can_lite.tracing_test PRIVATE
    TestCanBusMonitor.cpp
)
```

`can_lite.test_util` is defined in `can-lite/core/test/CMakeLists.txt`, which is added before
`tracing` as long as `add_subdirectory(tracing)` is last in `can-lite/CMakeLists.txt`.

Root `CMakeLists.txt` installs `can-lite/**/*.hpp` with `test` excluded — nothing to add there.

## 7. Tests — `can-lite/tracing/test/TestCanBusMonitor.cpp`

TDD: write these first, watch them fail, then implement.

### Harness

```cpp
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/tracing/CanBusMonitor.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "services/tracer/Tracer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::DoAll;
    using testing::SaveArg;
    using testing::StrictMock;

    hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes);   // copy the helper from TestCanFrameTransport.cpp

    class CanBusMonitorTest
        : public testing::Test
    {
    public:
        infra::StringOutputStream::WithStorage<512> stream;
        services::TracerToStream tracer{ stream };
        StrictMock<hal::CanMock> can;
        CanBusMonitor monitor{ can, tracer };
        ...
    };
}
```

Only `StrictMock` is allowed (CLAUDE.md). `hal::CanMock` comes from `can_lite.test_util` — do not
duplicate it. The constructor of `CanBusMonitor` must not call anything on `can`, so a `StrictMock`
with no expectations is valid at construction; `ReceiveData` is only forwarded when the monitor's own
`ReceiveData` is called.

**Every trace line is prefixed by `"\r\n"`** — `Tracer::StartTrace()` emits it before the payload
(confirmed by EMIL's own `TestTracer.cpp`: `tracer.Trace() << "Text"` yields `"\r\nText"`). Assert on
`stream.Storage()` with the `\r\n` included.

### Cases

| # | Test | Assertion |
|---|---|---|
| 1 | `SendDataIsForwardedToDelegate` | `EXPECT_CALL(can, SendData(...))` sees the same id and payload |
| 2 | `SendDataTracesDecodedExtendedFrame` | exact string for a known system frame, e.g. `MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, 0x123)` with payload `{0x01}` |
| 3 | `SendDataTracesStandardFrame` | 11-bit id → `"\r\nCAN TX standard id 0x123 dlc 2 data 0102"`; must not assert |
| 4 | `SuccessfulCompletionIsForwardedWithoutFailureTrace` | completion invoked with `true`, no `TX failed` in output |
| 5 | `FailedCompletionIsTracedAndForwarded` | output contains `"\r\nCAN TX failed"`, completion invoked with `false` |
| 6 | `SecondSendReusesCompletionSlotAfterCompletion` | send → complete → send again does not trip `really_assert` (documents the serialised contract) |
| 7 | `ReceiveDataRegistersWithDelegate` | `EXPECT_CALL(can, ReceiveData(_))` with `SaveArg<0>` |
| 8 | `ReceivedFrameIsTracedAndForwarded` | invoke the saved action; assert the `RX` line **and** that the registered action was called with the same id/data |
| 9 | `CommandAckFrameIsDecodedOnItsOwnLine` | system/`0x02` frame with payload `{0x03, 0x05, 0x04, 0x07}` produces the frame line followed by `"\r\nCAN ack cat 0x3 type 0x5 status sequenceError expectedSeq 0x7"` |
| 10 | `TruncatedCommandAckIsNotDecoded` | same frame with a 3-byte payload → frame line only, no `ack` line |
| 11 | `ApplicationCategoryAndUnknownMessageTypeAreNamedGenerically` | category `0x3`, type `0x42` → `... cat 0x3 application type 0x42 unknown ...` |
| 12 | `CategoryErrorMessageTypeIsNamedForAnyCategory` | type `0xfe` on an application category → `... type 0xfe categoryError ...` |
| 13 | `EmptyPayloadTracesZeroLengthData` | `dlc 0 data ` (trailing space, nothing after) |

Build the expected strings by hand and paste the literal — do **not** recompute the format in the test
with the same helpers the implementation uses, or the test proves nothing.

Note on tracing-disabled builds: with `EMIL_DISABLE_TRACING`, `Trace()` produces no output and these
assertions would fail. `EMIL_ENABLE_TRACING` defaults to `On` in EMIL's root `CMakeLists.txt` and this
repo does not override it, so the `host` presets are fine. Do not add a CMake override for it; just be
aware if someone later turns it off.

## 8. Documentation updates

Per CLAUDE.md's Document Consistency table. This change does **not** touch the wire format or protocol
behaviour, so:

- **Do not** add entries to `documents/requirements/can-protocol.yaml` or edit
  `documents/spec/can-protocol.md`. No requirement changes — this is a debug facility, not protocol.
- `documents/design/architecture.md`:
  - Add `tracing/` to the directory-structure block in §10.
  - Add a short section (§13, "Observability") covering: the decorator seam and why `hal::Can` was
    chosen, the wiring snippet from §1, the output format, and the note that it links `can_lite.core`
    plus `services.tracer` only. Update the `Last updated` date at the top.
- `README.md`: one bullet under **Features** and a `tracing/` line under **Project Structure**.

## 9. Definition of done

```bash
cmake --preset host
cmake --build --preset host-Debug
ctest --preset host
```

- `can_lite.tracing_test` builds and all its cases pass; the rest of the suite is unaffected.
- No warnings (they are errors).
- `grep -rn "GlobalTracer" can-lite/` returns nothing (goodcheck rule `amp.no-global-tracer`).
- No comments in any new source file.
- clang-format clean (`emil_clangformat_directories` already covers `can-lite`).

Commit on `claude/can-lite-instrumentation-tracing-j0uiuo`, push with
`git push -u origin claude/can-lite-instrumentation-tracing-j0uiuo`. Put the reasoning from §5.2 and
§5.3 (single completion slot, `AutoResetFunction` re-entrancy, single-chain `Trace()` requirement) in
the commit body — that is where it lives, since the code carries no comments. Do not open a PR unless
asked.

## 10. Deliberate non-goals (follow-up work, not this change)

- **Frame counters / statistics** (frames sent, received, send failures) exposed on the monitor.
- **A `CanFrameAnnotator` extension point** letting application categories register human-readable
  names for their own message types, replacing the generic `"application"` / `"unknown"` labels.
  Shape would be an `infra::IntrusiveList<CanFrameAnnotator>` on the monitor, matching the
  category-registration idiom already used by `CanProtocolServer`.
- **Wiring the monitor into `ApplicationFixture`** behind an opt-in flag, so a failing BDD scenario can
  dump the bus.
- **ISO-TP FSM state tracing** — a separate decorator on `IsoTpTransport`, the highest-value next
  target after this one.
