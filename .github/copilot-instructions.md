# can-lite — Copilot / AI agent instructions

This file is a concise, task-oriented guide for AI coding agents to be immediately productive in this repository.

1) Big-picture architecture (short)
- Purpose: Lightweight, extensible CAN bus protocol library implementing a client-server model over CAN 2.0B (29-bit extended identifiers). Designed for embedded systems with strict memory and timing constraints.
- Major components:
  - `can-lite/core/` — Protocol definitions (enums, CAN ID layout, constants), frame codec (fixed-point encoding), `CanPayload` (bounds-checked big-endian reader/writer), frame transport (async send queue), the base `CanCategory` hierarchy (`CanCategoryServer` / `CanCategoryClient`), `CanMessageHandler` (binds a message type ID to a member function), and `CanSequenceSource`.
  - `can-lite/categories/` — Built-in category implementations split into server/client pairs: `system/` (heartbeat, ack, discovery) and `firmware_upgrade/`. Application-specific categories belong in the consuming project.
  - `can-lite/server/` — Server implementation: listens for commands, dispatches to category handlers, sends acknowledgements. Uses observer pattern for application callbacks.
  - `can-lite/client/` — Client implementation: sends commands/queries to servers, receives responses. Supports multiple servers via node addressing; implements `CanSequenceSource`.
  - `can-lite/drivers/` — Hardware driver adapters.
  - `can-lite/transport/` — ISO-TP (ISO 15765-2) segmentation layer. All classes (`IsoTpSender`, `IsoTpReceiver`, `IsoTpChannelImpl`, `IsoTpTransportImpl`) are non-template with `WithStorage` aliases for zero-heap PDU buffer ownership.
  - `examples/` — Reference application categories (`foc_motor/`), not built by default (`CAN_LITE_BUILD_EXAMPLES`).
  - `embedded-infra-lib/` — Infrastructure dependency: bounded containers, build helpers, `hal::Can`, `infra::Subject`/`infra::SingleObserver`.
- Architecture: Client initiates all requests; Server listens and responds. Built-in System category (0x0) provides heartbeat, ack, status request, and category discovery. Categories are split into server/client pairs inheriting from `CanCategoryServer`/`CanCategoryClient` for compile-time type safety. All category handlers use `infra::Subject`/`infra::SingleObserver` for event notification. Applications extend via custom category implementations.
- Documents: `documents/spec/can-protocol.md` (wire-format spec), `documents/requirements/can-protocol.yaml` (formal requirements), `documents/design/architecture.md` (architecture & design decisions), `documents/design/extending-categories.md` (category authoring guide), `README.md` (project overview).

2) Critical developer workflows (exact commands)
- Clone:
  - `git clone --recursive <repo>`
- Configure & build host (recommended first step):
  - `cmake --preset host`
  - `cmake --build --preset host-Debug`
- Run unit tests (GoogleTest):
  - `ctest --preset host`
- Coverage/analysis presets are defined in `CMakePresets.json` — use `coverage` preset for coverage builds.

3) Project-specific constraints and conventions (must follow these)
- NO HEAP: avoid `new/delete`, `malloc/free`, `std::make_unique`, etc.
- NO dynamic STL containers: use `infra::BoundedVector`, `infra::BoundedDeque`, `infra::BoundedString`, `infra::Function`, etc. (see `embedded-infra-lib`).
- Prefer fixed-size integer types (`uint8_t`, `int32_t`, ...).
- Favor `constexpr`, `inline`, and `const` correctness.
- All multi-byte values on the wire are big-endian.
- **`WithStorage` pattern (EMIL convention)**: Concrete `Impl` classes that own sized storage must use `infra::WithStorage<ImplClass, StorageType>` with these rules:
  - The `Impl` class must NOT be templated on storage sizes — sizes live only in the `WithStorage` alias.
  - The `Impl` constructor takes a reference to the EMIL container (e.g. `infra::BoundedVector<T>&`) as its first argument — not a custom `Storage` struct.
  - Use `infra::BoundedVector<T>::WithMaxSize<N>` as the storage type for pool-style containers.
  - See `.github/instructions/embedded-cpp.instructions.md` for full examples.

4) Patterns & code locations (concrete examples)
- Add a new category (see `documents/design/extending-categories.md`):
  - Pick an application category ID (0x2–0xF) and declare it in a new `*Definitions.hpp` alongside its message type IDs.
  - Implement a `CanCategoryServer` subclass (takes a `CanFrameTransport&`) and/or a `CanCategoryClient` subclass (takes a `CanFrameTransport&` and a `CanSequenceSource&`).
  - Register the handler with `CanProtocolServer::RegisterCategory()` (server side) or `CanProtocolClient::RegisterCategory()` (client side).
  - Categories link `can_lite.core` only.
- Add a new message type to an existing category:
  - Add the message type ID `constexpr` in the category's `*Definitions.hpp`.
  - Add a private handler member function plus a `CanMessageHandler<Owner>` member binding the ID to it, and register it with `AddMessageTypes()` in the constructor.
  - Parse the payload with `CanPayloadReader` and notify via the category's observer.
- Fixed-point encoding: use `CanFrameCodec` helpers (`FloatToFixed16`, `Fixed16ToFloat`) or `CanPayloadWriter::WriteFixed16` / `CanPayloadReader::ReadFixed16`.
- Observer pattern: all categories expose events through `infra::Subject<Observer>` / `infra::SingleObserver`. Server-level events use `infra::Subject<CanProtocolServerObserver>`. Category-level events use per-category observer interfaces.

5) Testing & CI expectations
- Unit tests run on host using GoogleTest.
- Tests are in `can-lite/core/test/`, `can-lite/server/test/`, `can-lite/client/test/`, `can-lite/transport/test/`.
- Prefer small, deterministic tests that do not require hardware.
- Mock the `hal::Can` interface for protocol-level tests.

6) Build system tips
- Presets are the primary interface: see `CMakePresets.json`.
- Standalone builds fetch `embedded-infra-lib` automatically via FetchContent.
- When consumed as a subdirectory by another project, `embedded-infra-lib` must already be available.
- `compile_commands.json` is generated in build dirs; use it for language server/analysis.

7) When making changes, be explicit
- Update `documents/spec/can-protocol.md` and `documents/requirements/can-protocol.yaml` when changing protocol behavior.
- This library ships two built-in categories: System (0x0) and FirmwareUpgrade (0x1). Application-specific categories belong in consumer projects, not in this library, unless they are genuinely reusable protocol extensions.

8) Document consistency (must check before finishing any change)
- The following documents must stay aligned with each other and with the code:
  - `README.md` — project overview, quick-start, feature list.
  - `documents/spec/can-protocol.md` — wire-format specification (CAN ID layout, message types, encoding).
  - `documents/requirements/can-protocol.yaml` — formal requirements (traceability to spec and tests).
  - `documents/design/architecture.md` — architecture decisions, patterns, component relationships.
  - `documents/design/extending-categories.md` — category authoring guide.
- After any protocol, structural, or behavioral change, review all five documents for consistency. Update any document that contradicts or omits the new behavior.
- Pay particular attention to: CAN ID bit-field definitions, category IDs, message type values, observer interfaces, and the category hierarchy description.

9) Quick pointers for reviewers / code suggestions
- If suggesting new APIs, prefer interface-driven DI and small, testable functions.
- Observer callbacks must not allocate or block.

10) Documentation rules (apply to every document)

These apply to every document under `documents/`, to `README.md`, and to any
new document.

- **Single source of truth.** Each fact lives in exactly one document:
  wire format and message catalogues in `documents/spec/`, formal requirements
  in `documents/requirements/`, architecture decisions in
  `documents/design/architecture.md`, category authoring in
  `documents/design/extending-categories.md`, layer narrative, diagrams and
  corner cases in `documents/booklet/`.
- **Cross-reference, never restate.** If a table, identifier layout, message
  catalogue or enumeration already exists in another document, link to it. A
  second copy drifts from the first.
- **No code in documentation.** Documents describe design and behaviour, not
  source. No code excerpts, listings or snippets — name the components and
  their responsibilities instead; a reader who needs the code opens the code.
- **Diagrams earn their place.** A class, sequence, state or flow diagram that
  is not already in another document is the one thing worth adding.
- **Fix, do not fork.** When a change makes a document wrong, correct that
  document; never add a corrected copy somewhere else.
