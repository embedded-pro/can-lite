# can-lite — Agent Rules

<!-- shared:begin 0.3.0 sha256:9aee198041110762 -->
Canonical rules for Claude, Copilot and sub-agents. Sections marked *(shared)* come from embedded-pro/embedded-agent-kit:
edit them there, not here. *(repo)* sections belong to this repository. *Exceptions* overrides a shared rule.

## Behaviour *(shared)*

- **Be as terse as possible.** No preamble, summary or narration. Report file paths and pass/fail.
- Everything you write — code, docs, agent files, PR text — uses the fewest words that stay unambiguous.
- Prose (docs, comments, agent files, commit and PR text) follows ASD-STE100 Simplified Technical English:
  one word, one meaning; active voice; one instruction per sentence; sentences ≤ 20 words (procedures) or ≤ 25 (descriptions).
- Ambiguous request: ask. Clear request: act.
- Do only what was asked. No unrelated refactors, no extra features.
- Batch reads, don't re-read, prefer targeted edits.
- Never change a shared section. Add a deviation to *Exceptions* only when the user approves it.

## Workflow *(shared)*

1. Multi-file or architectural change: update the design document first. Get approval before you write code.
2. Read *Domain* and *Build*. Read each `.github/instructions/*.instructions.md` whose `applyTo` matches a file you change.
3. Find the nearest existing pattern. Follow it.
4. Per behaviour: write a failing test, write the minimum code that passes it, refactor with all tests green.
5. Add each new file to its `CMakeLists.txt`.
6. Update the documentation for each behavioural change.
7. Run the *Build* commands until build and tests pass. Never disable, skip or weaken a test to get green.
8. Non-trivial change: run the `reviewer` agent, or ask the user to run it. Fix each CRITICAL and WARNING finding.
9. Report changed files, build pass/fail, test pass/fail.

## Memory *(shared)*

Scope: production code. Tests and host tools may use the heap.

- Forbidden: `new`/`delete`/`malloc`/`free`, `make_unique`/`make_shared`,
  `std::vector`/`string`/`deque`/`list`/`map`/`set`, recursion.
- Use: `infra::BoundedVector<T>::WithMaxSize<N>`, `infra::BoundedString::WithStorage<N>`,
  `infra::BoundedDeque`, `infra::BoundedList`/`IntrusiveList`, `std::array`, `std::optional`, `std::variant`.

## Interfaces & errors *(shared)*

- No virtual destructors: each adds a deleting destructor and thunks to flash.
  Interface = pure virtual methods + `protected: ~I() = default;` (non-virtual).
- Public `virtual ~Base() = default;` only on a base destroyed through a base pointer:
  `delete`/`std::unique_ptr<Base>`, `infra::PolymorphicVariant<Base, ...>`, `infra::OptionalForPolymorphicObjects<Base, ...>`.
  Put it on that base, not on every interface.
- Not needed for `std::variant`, `std::optional<Concrete>`, `infra::SharedPtr`: they destroy the concrete type.
- No exceptions. `std::optional`, `bool` or status enum; `really_assert` for preconditions. Never swallow an error.
- Constructor injection; no global state.

## Optimisation *(shared)*

- No `#pragma GCC optimize`, no `optimize` attribute: GCC won't inline across differing options.
  The consumer sets flags per translation unit.
- `OPTIMIZE_FOR_SPEED` only on hot functions defined in a header.
- Hot paths: no heap, no virtual dispatch, no blocking.

## Style *(shared)*

- `.clang-format` is authoritative. Allman, 4 spaces, `{}` init.
- PascalCase types/methods; camelCase members and enum values; lowercase namespaces.
  No `I` prefix; implementations end in `Impl`.
- `#pragma once` only.
- Fixed-width integers. `const`/`constexpr`-correct. Functions ≤ 30 lines (hard limit 50).
- Comments: non-obvious *why* only, one line. No `TODO`/`FIXME`, commented-out code, or restating the code.
  Fix or delete a comment the change makes stale.

## Testing *(shared)*

- `TEST_F` for fixtures, `TYPED_TEST` for types, plain `TEST()` for stateless checks.
- `StrictMock` only. `EXPECT_NEAR` with explicit tolerance. Fixture in an anonymous namespace, macros outside.
- Heap allowed in tests.

## Documentation *(shared)*

- Update docs before or with any behavioural change.
- One fact, one place: link, never restate.
- Visuals: Mermaid or ASCII.
<!-- shared:end -->

## Domain *(repo)*

- Client-server CAN protocol library over CAN 2.0B (29-bit extended identifiers only). Namespace `services`.
- Topology: one client, many servers. A server serves one client and keeps one sequence counter (REQ-CAN-006.1).
- Layer stack, top to bottom:

  ```text
  Application Layer  (consumer code; application categories)
  Category Layer     (system/0x00, firmware_upgrade/0x01)
  Protocol Layer     (CanProtocolServer / CanProtocolClient)
  Transport Layer    (IsoTpTransportImpl — ISO 15765-2, optional)
  Core Layer         (CanFrameTransport, CanFrameCodec, CanPayload, CanCategory, CanMessageHandler)
  HAL                (hal::Can)
  ```

- Layout under `can-lite/`:
  - `core/` — `CanProtocolDefinitions.hpp` (ID layout, enums, constants), `CanFrameCodec` (fixed-point),
    `CanPayload` (bounds-checked big-endian reader/writer), `CanFrameTransport` (async send queue),
    `CanCategoryServer`/`CanCategoryClient`, `CanMessageHandler`, `CanSequenceSource`.
  - `categories/` — built-in server/client pairs: `system/` (heartbeat, ack, discovery), `firmware_upgrade/`.
    Application categories belong in the consumer, not here.
  - `server/`, `client/` — `CanProtocolServer`/`CanProtocolClient`: dispatch, sequence tracking, liveness,
    optional ISO-TP (`server.AttachIsoTpTransport(isoTp)`). The client implements `CanSequenceSource`.
  - `transport/` — ISO-TP (`IsoTpTransportImpl`); non-template classes with `WithStorage` aliases.
  - `drivers/` — hardware and virtual bus adapters. `tracing/` — tracing decorators.
- `examples/` — reference application categories (`setpoint/`). Off by default: `-DCAN_LITE_BUILD_EXAMPLES=On`;
  forced on with tests.
- CAN ID layout (29-bit extended):

  ```text
  [28:24] Priority (5 bits): Emergency=0, Command=4, Response=8, Telemetry=12, Heartbeat=16
  [23:20] Category (4 bits): System=0x0, FirmwareUpgrade=0x1, application=0x2–0xF
  [19:12] Message Type (8 bits): commands 0x00–0x7F, responses 0x80–0xFF (0xFE reserved for category error)
  [11:0]  Node ID (12 bits): 0x000=broadcast, 0x001–0xFFF=individual
  ```

- Build and read IDs with `CanProtocolDefinitions::MakeCanId()` and `ExtractCan*()`. No manual bit shifts.
- Wire format: multi-byte values big-endian. Max 8 bytes per frame. Encode with `CanFrameCodec`
  (`FloatToFixed16`, `WriteInt16`, ...) and `CanPayloadReader`/`CanPayloadWriter` (sticky `Valid()`),
  never manual byte offsets.
- Category pattern (guide: `documents/design/extending-categories.md`):
  - Server: inherits `CanCategoryServer` + `infra::Subject<Observer>`, takes `CanFrameTransport&`.
    Handles commands `0x00–0x7F`; sends with `SendResponse()`, `SendTelemetry()`, `SendCategoryError()`,
    `SendCommandAck()`.
  - Client: inherits `CanCategoryClient` + `infra::Subject<Observer>`, takes `CanFrameTransport&` and
    `CanSequenceSource&`. Handles responses `0x80–0xFF`; sends with `SendCommand(nodeId, messageType[, payload]
    [, priority])`, which prepends the sequence byte. `SendCommandWithoutSequence()` only when the paired server's
    `RequiresSequenceValidation()` is `false`.
  - Sequence validation: on by default for servers (`data[0]`, so handlers `Skip(1)` first), off for clients.
  - Message types: `CanMessageHandler<Owner>` members binding an ID to a member function, registered with
    `AddMessageTypes(...)` in the constructor. No nested `CanMessageType` subclass per message.
  - Handlers return `true` (accepted) or `false` (invalid payload). On `false` the server acks `invalidPayload`;
    handlers never send that ack.
  - Categories link `can_lite.core` only, never `can_lite.client` or `can_lite.server`.
- New category: pick an ID `0x2–0xF` as `constexpr` in a new `*Definitions.hpp` with its message type IDs; write the
  server and client (reference: `categories/firmware_upgrade/`, `integration_tests/support/TestCategories.hpp`);
  register with `server.RegisterCategory(...)` and `client.RegisterCategory(...)`.
- New message type: add its ID to the category's `*Definitions.hpp`, add a handler and its `CanMessageHandler`
  member, register it, parse with `CanPayloadReader`, notify the observer.
- `WithStorage` (EMIL): `Impl` classes are non-template; sizes live only in the nested alias. The constructor takes
  a reference to the EMIL container first; `infra::WithStorage` owns it.

  ```cpp
  template<std::size_t N>
  using WithStorage = infra::WithStorage<FooImpl,
      typename infra::BoundedVector<Item>::template WithMaxSize<N>>;
  explicit FooImpl(infra::BoundedVector<Item>& items, Bar& bar);
  // FooImpl::WithStorage<8> foo{ bar };
  ```

- Observers: `infra::SingleObserver<Observer, Subject>`, one per subject, attach on construction. Callbacks never
  allocate or block.
- Execution model: no blocking, sleep or busy-wait. Schedule with `infra::EventDispatcher::Instance().Schedule()`;
  callbacks are `infra::Function<void()>`.
- Unit tests: `can-lite/{module}/test/Test{ComponentName}.cpp`; doubles in `test/` or `test_doubles/`. Mock
  `hal::Can` for protocol tests. Each test traces to a requirement in `documents/requirements/`. Cover empty and
  8-byte payloads, boundaries, invalid input, server sequence errors and observer notifications.
- Integration tests (cucumber-cpp-runner): `integration_tests/features/*.feature`, `steps/*.cpp`.
  `support/ApplicationFixture.hpp` composes `VirtualCan` pairs, server, client and `StrictMock` observers; get it
  with `context.Get<ApplicationFixture>()`; advance timers with `ForwardTime()`. `support/TestCategories.hpp` holds
  `DemoCategory` (`0x3`). Never capture a `shared_ptr` to the fixture in a lambda stored on the fixture.
- Documents, one fact each: wire format in `documents/spec/`, requirements in `documents/requirements/`,
  architecture in `documents/design/architecture.md`, category authoring in
  `documents/design/extending-categories.md`, layer narrative, diagrams and corner cases in `documents/booklet/`.
- No source code in documents: name components instead. Commands to run and diagram sources are allowed.
- A diagram earns its place only when no other document has it. A wrong document is fixed, never forked.
- After a protocol, structural or behavioural change, align: `documents/spec/can-protocol.md`,
  `documents/requirements/can-protocol.yaml`, `documents/design/architecture.md`, `documents/booklet/*.md`,
  `README.md`. Category documents sit beside them (`spec/firmware-upgrade.md`,
  `requirements/firmware-upgrade.yaml`); examples keep theirs in `examples/<name>/documents/`.
- Booklet: one chapter per file in `documents/booklet/`, ordered by its `README.md` index table, which drives the
  PDF and the site. `scripts/build-booklet.py` writes `build/booklet/CanLiteDesign.pdf` and `build/booklet/site/`;
  `build-booklet.yml` publishes the site on `main` and attaches the PDF to releases.
- Booklet holds only composition, pipelines, state machines, corner cases, budgets and verification; it links the
  rest. Chapters number their own sections (`## 1.`). Add a chapter only when no existing document fits.
- Booklet diagrams: Mermaid, rendered by `mmdc`; a parse error fails the build. Avoid `;` in sequence notes and
  `{}` in `classDiagram` members.
- CAN standards (UDS, J1939, ISO-TP, CANopen) and their mapping to categories:
  `.github/instructions/can-bus-protocols.instructions.md`.
- Standalone builds fetch `embedded-infra-lib` through FetchContent. As a subdirectory, the consumer provides it.
- CMake libraries: `can_lite.<component>`.

## Build *(repo)*

```bash
cmake --preset host && cmake --build --preset host-Debug
ctest --preset host
cmake --preset coverage && cmake --build --preset coverage && ctest --preset coverage
```

Presets: `host`, `host-single-Debug`, `coverage`, `windows-bootstrap` (once, before `windows`), `windows`.
Warnings are errors. Single test binary: `./build/host/can-lite/<module>/test/Debug/<binary>`.

## Exceptions *(repo)*

- No comments at all, not even a non-obvious *why*: put the reason in the commit message or PR description.
- No virtual destructor in production code, not even on a base deleted through a base pointer: nothing here is.
  Test fixtures deriving from `testing::Test` are the only exception.
