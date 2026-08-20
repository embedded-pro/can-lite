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
Application Layer  (consumer code; application categories)
Category Layer     (system/0x00, firmware_upgrade/0x01)
Protocol Layer     (CanProtocolServer / CanProtocolClient)
Transport Layer    (IsoTpTransportImpl — ISO 15765-2, optional)
Core Layer         (CanFrameTransport, CanFrameCodec, CanPayload, CanCategory, CanMessageHandler)
HAL                (hal::Can)
```

### Key Components

- **`can-lite/core/`** — `CanProtocolDefinitions.hpp` (CAN ID layout, enums, constants), `CanFrameCodec` (fixed-point encode/decode), `CanPayload` (bounds-checked big-endian reader/writer), `CanFrameTransport` (async send queue), `CanCategory` base hierarchy (`CanCategoryServer`/`CanCategoryClient`), `CanMessageHandler` (binds a message type ID to a member function), `CanSequenceSource` (per-server sequence supply).
- **`can-lite/categories/`** — Server/client pairs shipped with the library: `system/` (heartbeat, ack, discovery), `firmware_upgrade/`. Each pair has its own `*Definitions.hpp` with category ID and message type IDs. **Application-specific categories belong in the consuming project, not here.**
- **`can-lite/server/`** and **`can-lite/client/`** — `CanProtocolServer`/`CanProtocolClient` handle dispatch, sequence tracking, liveness detection, and optional ISO-TP attachment. `CanProtocolClient` implements `CanSequenceSource`.
- **`can-lite/transport/`** — ISO-TP layer (`IsoTpTransportImpl`); all classes are non-template with `WithStorage` aliases. Attach via `server.AttachIsoTpTransport(isoTp)`.
- **`examples/`** — Reference application categories (`foc_motor/`). Not built by default; enable with `-DCAN_LITE_BUILD_EXAMPLES=On` (forced On when tests are enabled).
- **`integration_tests/`** — BDD tests (cucumber-cpp-runner); `support/ApplicationFixture.hpp` composes `VirtualCan` pairs, server, client, and `StrictMock` observers. `support/TestCategories.hpp` holds the `DemoCategory` server/client pair (`0x3`) used as the minimal reference category.

### CAN ID Layout (29-bit extended)

```
[28:24] Priority (5 bits): Emergency=0, Command=4, Response=8, Telemetry=12, Heartbeat=16
[23:20] Category (4 bits): System=0x0, FirmwareUpgrade=0x1, application=0x2–0xF
[19:12] Message Type (8 bits): commands 0x00–0x7F, responses 0x80–0xFF (0xFE reserved for category error)
[11:0]  Node ID (12 bits): 0x000=broadcast, 0x001–0xFFF=individual
```

Use `CanProtocolDefinitions::MakeCanId()` and `ExtractCan*()` helpers — never manual bit shifts in category code.

Topology is **one client to many servers**. A server serves exactly one client and keeps a single sequence counter (REQ-CAN-006.1).

### Category Pattern

Every category is a **server/client pair**. Full guide: `documents/design/extending-categories.md`.

- Server inherits `CanCategoryServer` + `infra::Subject<MyServerObserver>`. Takes a `CanFrameTransport&`. Registers command handlers (`0x00–0x7F`); sends via the inherited `SendResponse()` / `SendTelemetry()` / `SendCategoryError()` / `SendCommandAck()`.
- Client inherits `CanCategoryClient` + `infra::Subject<MyClientObserver>`. Takes a `CanFrameTransport&` and a `CanSequenceSource&`. Registers response handlers (`0x80–0xFF`); sends via the inherited `SendCommand(nodeId, messageType[, payload][, priority])`, which prepends the sequence byte. Use `SendCommandWithoutSequence()` only when the paired server sets `RequiresSequenceValidation()` to `false`.
- Message types are `CanMessageHandler<Owner>` members binding an ID to a member function, registered with `AddMessageTypes(...)` in the constructor. Do not write a nested `CanMessageType` subclass per message.
- Payloads use `CanPayloadReader` / `CanPayloadWriter` (big-endian, bounds-checked, sticky `Valid()`), not manual byte offsets.
- Sequence validation: server categories default `true` (byte `data[0]`, so server command handlers `Skip(1)` before reading), client categories default `false`.
- Observer interfaces use `infra::SingleObserver<Observer, Subject>` — one observer per subject; auto-attaches/detaches on construction/destruction.

To add a new category:
1. Pick a category ID (0x2–0xF for application categories) and add a `constexpr` in the new `*Definitions.hpp`.
2. Implement `*CategoryServer` and `*CategoryClient`; use `categories/firmware_upgrade/` or `integration_tests/support/TestCategories.hpp` as the reference.
3. Register with `server.RegisterCategory(myCategoryServer)` and `client.RegisterCategory(myCategoryClient)`.

Categories link `can_lite.core` only — never `can_lite.client` or `can_lite.server`.

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
- `ApplicationFixture` (in `support/ApplicationFixture.hpp`) provides `VirtualCan` pairs, `CanProtocolServer`/`Client`, and `StrictMock` observers. Inherits `infra::ClockFixture` — use `ForwardTime()` to advance timers.
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
- **No comments.** Do not add explanatory, rationale, or section-marker comments to code, headers, or tests. Code must read clearly from names and structure alone. This applies even when the reasoning behind a line is non-obvious — put that in the commit message or PR description, not in the file.

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
| `documents/booklet/*.md`                   | Design booklet chapters (layers, class/sequence diagrams, corner cases) |
| `README.md`                                | Project overview, features          |

Category-specific specs and requirements live alongside the main ones: `documents/spec/firmware-upgrade.md`, `documents/requirements/firmware-upgrade.yaml`. The category authoring guide is `documents/design/extending-categories.md`. Example categories keep their docs under `examples/<name>/documents/`.

### Design booklet

`documents/booklet/` holds the design booklet: one chapter per file, ordered by the table in `documents/booklet/README.md`. `scripts/build-booklet.py` assembles it into `build/booklet/CanLiteDesign.pdf` and a multi-page static site under `build/booklet/site/`; `.github/workflows/build-booklet.yml` publishes the site to GitHub Pages on `main` and attaches the PDF to every published release.

- Diagrams are ```` ```mermaid ```` fences, rendered to SVG with `mermaid-cli` (`mmdc`) at build time. A fence that fails to parse fails the build — avoid `;` in sequence-diagram notes and `{}` inside `classDiagram` members.
- Chapters carry their own section numbering (`## 1.`, `## 2.`); LaTeX numbers chapters only.
- Add a chapter by creating the file and linking it from the index table under the right `## Part ...` heading — the index drives both outputs.
- After a protocol, structural or behavioural change, update the affected booklet chapter along with the documents in the table above.

## Build System Notes

- CMake presets are the primary interface — see `CMakePresets.json`.
- Library naming: `can_lite.<component>` (e.g., `can_lite.core`, `can_lite.server`).
- Standalone builds fetch `embedded-infra-lib` automatically via FetchContent. When consumed as a subdirectory, `embedded-infra-lib` must already be available.
