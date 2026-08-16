# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test Commands

```bash
# Configure
cmake --preset host

# Build (Debug)
cmake --build --preset host-Debug

# Run all unit tests
ctest --preset host

# Run single-config tests (alternative preset)
cmake --preset host-single-Debug && cmake --build --preset host-single-Debug
ctest --preset host-single-Debug

# Coverage build
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage

# Run a single test binary directly (after build)
./build/host/Debug/<test-binary>
```

Warnings are treated as errors (`CMAKE_COMPILE_WARNING_AS_ERROR=On`). `compile_commands.json` is generated in each build directory.

## Architecture Overview

### Layer Stack

```
Application Layer  (consumer code; consumer-owned categories 0x2-0x7)
Category Layer     (system/0x0, firmware_upgrade/0x1)
Protocol Layer     (CanProtocolServer / CanProtocolClient)
Transport Layer    (IsoTpTransportImpl — ISO 15765-2, optional)
Core Layer         (CanFrameTransport, CanFrameCodec, CanCategory)
HAL                (hal::Can)
```

### Key Components

- **`can-lite/core/`** — `CanProtocolDefinitions.hpp` (CAN ID layout, enums, constants), `CanFrameCodec` (fixed-point encode/decode), `CanFrameTransport` (async send queue), `CanCategory` base hierarchy (`CanCategoryServer`/`CanCategoryClient`) and handler binding, `CanCategoryOutbound` (per-category outbound handle), `CanSequenceTable` (per-peer sequence state).
- **`can-lite/categories/`** — Management server/client pairs only: `system/` (heartbeat, ack, discovery) and `firmware_upgrade/`. Each pair has its own `*Definitions.hpp` with category ID and message type IDs.
- **`can-lite/testing/`** — `can_lite.testing`: the generic echo category (the reference example for a consumer category) and `VirtualCan`, a two-node in-memory bus.
- **`can-lite/server/`** and **`can-lite/client/`** — `CanProtocolServer`/`CanProtocolClient` handle dispatch, sequence tracking, liveness detection, and optional ISO-TP attachment.
- **`can-lite/transport/`** — ISO-TP layer (`IsoTpTransportImpl`); all classes are non-template with `WithStorage` aliases. Attach via `server.AttachIsoTpTransport(isoTp)`.
- **`integration_tests/`** — BDD tests (cucumber-cpp-runner); `support/ApplicationFixture.hpp` composes `VirtualCan` pairs, server, client, and `StrictMock` observers.

### CAN ID Layout (29-bit extended)

```
[28:24] Priority (5 bits): Emergency=0, Command=4, Response=8, Telemetry=12, Heartbeat=16
[23:20] Category (4 bits): System=0x0, FirmwareUpgrade=0x1, custom 0x2-0x7, reserved 0x8-0xF
[19:12] Message Type (8 bits): commands 0x00–0x7F, responses 0x80–0xFF
[11:0]  Node ID (12 bits): 0x000=broadcast, 0x001–0xFFF=individual
```

Use `CanProtocolDefinitions::MakeCanId()` and `ExtractCan*()` helpers — never manual bit shifts in category code. In practice a category composes no identifier at all: it sends through its `CanCategoryOutbound` handle.

The wire layout is fixed. The 0x2-0x7 custom range is a **protocol invariant enforced at registration**, not an encoding change: at most 8 categories per node, which is exactly what one category-discovery frame holds.

### What Belongs In can-lite

A category belongs in can-lite **if and only if** it concerns the node as a protocol participant or as a device, and is agnostic to what the device does. A category that ascribes meaning to the payload in application terms belongs to the consumer. System (0x0) and Firmware Upgrade (0x1) qualify; motor control, sensing and actuation do not. Add new application categories to consumer projects, never here.

### Category Pattern

Every category is a **server/client pair**:

- Server inherits `CanCategoryServer` + `infra::Subject<MyServerObserver>`. Registers command handlers (`0x00–0x7F`), provides `Send*Response()` methods.
- Client inherits `CanCategoryClient` + `infra::Subject<MyClientObserver>`. Registers response handlers (`0x80–0xFF`), provides `Send*Command(nodeId, …)` methods.
- Message types are bound via `AddMessageType(messageTypeId, handler)` in the constructor, where `handler` is `infra::Function<bool(infra::ConstByteRange)>`. Storage comes from `CanCategoryHandlerStorage<Max>`, derived from **privately and first** so it outlives the base's reference to it. There is no class per message type.
- A handler returns `true` when it accepted the payload and `false` when it rejected it; the host turns a rejection into an `invalidPayload` acknowledgement and an unrecognised message type into `unknownCommand`.
- Sequence validation: `RequiresSequenceValidation()` is pure virtual, so each category declares its own policy. Server categories normally answer `true` (validates `data[0]`); client categories normally answer `false`.
- Sending goes through `Outbound()`, never through a `CanFrameTransport&` member. A category library therefore links only `can_lite.core`.
- Observer interfaces use `infra::SingleObserver<Observer, Subject>` — one observer per subject; auto-attaches/detaches on construction/destruction.

To add a new category (in a consumer project):
1. Pick a category ID in the integrator range (0x2–0x7) and pass it to the constructor. Do **not** hard-code it as a `constexpr` — the integrator owns the assignment.
2. Implement `*CategoryServer` and `*CategoryClient` following `can-lite/testing/EchoCategoryServer.hpp` / `EchoCategoryClient.hpp` as the reference.
3. Register with `server.RegisterCategory(myCategoryServer)` and `client.RegisterCategory(myCategoryClient)` from your composition root. There is no plugin registry and no self-registration.

### WithStorage Pattern (EMIL convention)

`Impl` classes are non-template; sizes live only in the `WithStorage` nested alias:

```cpp
class FooImpl : public Foo
{
public:
    template<std::size_t N>
    using WithStorage = infra::WithStorage<FooImpl,
        typename infra::BoundedVector<Item>::template WithMaxSize<N>>;

    explicit FooImpl(infra::BoundedVector<Item>& items, Bar& bar);
};
// Instantiate: FooImpl::WithStorage<8> foo{ bar };
```

The `Impl` constructor takes a **reference to the EMIL container** as its first argument. `infra::WithStorage` privately owns the storage and passes it as that first argument.

### Observer Pattern

```cpp
class MyServerObserver
    : public infra::SingleObserver<MyServerObserver, MyCategoryServer>
{
    virtual void OnCommand(/* params */) = 0;
};

class MyCategoryServer
    : public CanCategoryServer
    , public infra::Subject<MyServerObserver>
{ ... };
```

Observer callbacks must not allocate or block.

### Integration Tests

- Feature files: `integration_tests/features/*.feature` (Gherkin)
- Step definitions: `integration_tests/steps/*.cpp`
- `ApplicationFixture` (in `support/ApplicationFixture.hpp`) provides `VirtualCan` pairs, `CanProtocolServer`/`Client`, `StrictMock` observers and `RegisterEchoCategory(id, requiresSequenceValidation)`. Inherits `infra::ClockFixture` — use `ForwardTime()` to advance timers. It knows about no domain category: a scenario that needs one emplaces its own fixture (see `steps/FirmwareUpgradeFixture.hpp`).
- Retrieve the fixture in steps via `context.Get<ApplicationFixture>()`.
- Never capture `shared_ptr` to the fixture inside lambdas stored on the fixture (circular reference).

## Embedded C++ Constraints (Non-negotiable)

**No heap allocation** — forbidden: `new`, `delete`, `malloc`, `free`, `std::make_unique`, `std::make_shared`, `std::vector`, `std::string`, `std::deque`, `std::list`, `std::map`, `std::set`.

**Required replacements:**
- `std::vector<T>` → `infra::BoundedVector<T>::WithMaxSize<N>`
- `std::string` → `infra::BoundedString::WithStorage<N>`
- `std::deque<T>` → `infra::BoundedDeque<T>::WithMaxSize<N>`
- `std::list<T>` → `infra::IntrusiveList<T>`
- `std::optional<T>` for values that may be absent
- `std::array<T, N>` for fixed-size arrays

**Execution model:** no blocking, no sleep, no busy-wait. Schedule async completions with `infra::EventDispatcher::Instance().Schedule()`. Use `infra::Function<void()>` for callbacks.

**Wire format:** all multi-byte values **big-endian**. Use `CanFrameCodec` helpers (`FloatToFixed16`, `WriteInt16`, `ReadInt16`, etc.). Max 8 bytes per CAN 2.0 frame.

## Style & Naming

- **Classes/methods:** `PascalCase`
- **Member variables/enum values:** `camelCase`
- **Namespaces:** lowercase — **`services`** (the codebase uses `services`, not `can_lite`)
- **Interfaces/abstract classes:** plain name (no `I` prefix). `IsoTpTransport`, not `IIsoTpTransport`.
- **Concrete implementations:** plain name + `Impl` suffix.
- Allman brace style, 4-space indent.
- `{}` brace initialization everywhere: `uint8_t count{}`, `MyClass obj{arg1, arg2}`.
- Fixed-size integer types: `uint8_t`, `int32_t`, etc.
- `const` on all non-mutating methods; `constexpr` for compile-time values.
- Functions ≤ 30 lines (hard limit 50).
- No pure virtual destructors unless the class is deleted polymorphically through a base pointer (adds vtable overhead).
- Error handling: `std::optional<T>` or status enums — no exceptions.

## Testing Rules

- **Unit tests:** GoogleTest + GoogleMock in `can-lite/{module}/test/Test{ComponentName}.cpp`.
- **Only `testing::StrictMock<>` is allowed.** `NiceMock`, `NaggyMock`, and bare mock classes are forbidden.
- Mock `hal::Can` for protocol-level tests.
- TDD: write failing tests before implementation (Red → Green → Refactor).
- Each test traces to a requirement in `documents/requirements/`.
- Integration test observers must also be `StrictMock`.

## Document Consistency

After any protocol, structural, or behavioral change, keep these aligned:

| Document                                   | Covers                              |
|--------------------------------------------|-------------------------------------|
| `documents/spec/can-protocol.md`           | Wire-format specification           |
| `documents/requirements/can-protocol.yaml` | Formal protocol requirements        |
| `documents/design/architecture.md`         | Architecture decisions and patterns |
| `README.md`                                | Project overview, features          |

Category-specific specs and requirements live alongside the main ones: `documents/spec/firmware-upgrade.md` and `documents/requirements/firmware-upgrade.yaml`.

## Build System Notes

- CMake presets are the primary interface — see `CMakePresets.json`.
- Library naming: `can_lite.<component>` (e.g., `can_lite.core`, `can_lite.server`).
- Standalone builds fetch `embedded-infra-lib` automatically via FetchContent. When consumed as a subdirectory, `embedded-infra-lib` must already be available.
