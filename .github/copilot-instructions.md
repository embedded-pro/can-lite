# can-lite — Copilot / AI agent instructions

This file is a concise, task-oriented guide for AI coding agents to be immediately productive in this repository.

1) Big-picture architecture (short)
- Purpose: Lightweight, extensible CAN bus protocol library implementing a client-server model over CAN 2.0B (29-bit extended identifiers). Designed for embedded systems with strict memory and timing constraints.
- Major components:
  - `can-lite/core/` — Protocol definitions (enums, CAN ID layout, constants), frame codec (fixed-point encoding), frame transport (async send queue), the base `CanCategory` hierarchy (`CanCategoryServer` / `CanCategoryClient`) with its handler binding, the per-category outbound handle (`CanCategoryOutbound`) and per-peer sequence state (`CanSequenceTable`).
  - `can-lite/categories/` — Management category implementations split into server/client pairs: `system/` (heartbeat, ack, discovery) and `firmware_upgrade/`.
  - `can-lite/testing/` — `can_lite.testing`: the generic echo category, which is THE reference example for a consumer-owned category, plus `VirtualCan`, a two-node in-memory bus for host tests.
  - `can-lite/server/` — Server implementation: listens for commands, dispatches to category handlers, sends acknowledgements. Uses observer pattern for application callbacks.
  - `can-lite/client/` — Client implementation: sends commands/queries to servers, receives responses. Supports multiple servers via node addressing.
  - `can-lite/drivers/` — Hardware driver adapters.
  - `can-lite/transport/` — ISO-TP (ISO 15765-2) segmentation layer. All classes (`IsoTpSender`, `IsoTpReceiver`, `IsoTpChannelImpl`, `IsoTpTransportImpl`) are non-template with `WithStorage` aliases for zero-heap PDU buffer ownership.
  - `embedded-infra-lib/` — Infrastructure dependency: bounded containers, build helpers, `hal::Can`, `infra::Subject`/`infra::SingleObserver`.
- Architecture: Client initiates all requests; Server listens and responds. Built-in System category (0x0) provides heartbeat, ack, status request, and category discovery. Categories are split into server/client pairs inheriting from `CanCategoryServer`/`CanCategoryClient` for compile-time type safety. All category handlers use `infra::Subject`/`infra::SingleObserver` for event notification. Applications extend via their own category implementations, registered by explicit `RegisterCategory` calls from a composition root — there is no plugin registry and no self-registration.
- Category ID policy: `0x0-0x1` management (owned by can-lite), `0x2-0x7` integrator-assigned, `0x8-0xF` reserved. Max 8 categories per node. The wire layout is unchanged — the range is a protocol invariant enforced at registration.
- Documents: `documents/spec/can-protocol.md` (wire-format spec), `documents/requirements/can-protocol.yaml` (formal requirements), `documents/design/architecture.md` (architecture & design decisions), `README.md` (project overview).

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
- Add a new message category (in a consumer project, following `can-lite/testing/EchoCategoryServer.hpp` / `EchoCategoryClient.hpp`):
  - Pick an ID in the integrator range (`0x2-0x7`) and take it as a **constructor parameter** — do not hard-code it as a `constexpr`.
  - Implement a `CanCategoryServer` subclass and/or a `CanCategoryClient` subclass, deriving privately and first from `CanCategoryHandlerStorage<Max>`.
  - Send through `Outbound()`; a category holds no `CanFrameTransport&` and no protocol host, so it links only `can_lite.core`.
  - Register with `CanProtocolServer::RegisterCategory()` (server side) or `CanProtocolClient::RegisterCategory()` (client side).
- Add a new message type to an existing category:
  - Add a message type ID constant in that category's `*Definitions.hpp`.
  - Bind it with `AddMessageType(messageTypeId, handler)` in the category constructor, where `handler` is `infra::Function<bool(infra::ConstByteRange)>`.
  - Return `true` when the payload was accepted and `false` when it was rejected; notify via the category's observer. The host converts a rejection into an `invalidPayload` acknowledgement and an unrecognised message type into `unknownCommand`.
- Fixed-point encoding: use `CanFrameCodec` helpers (`FloatToFixed16`, `Fixed16ToFloat`, `WriteInt16`, `ReadInt16`, etc.).
- Observer pattern: all categories expose events through `infra::Subject<Observer>` / `infra::SingleObserver`. Server-level events use `infra::Subject<CanProtocolServerObserver>`. Category-level events use per-category observer interfaces (e.g. `EchoCategoryServerObserver`).

5) Testing & CI expectations
- Unit tests run on host using GoogleTest.
- Tests are in `can-lite/core/test/`, `can-lite/server/test/`, `can-lite/client/test/`, `can-lite/transport/test/`, `can-lite/testing/test/`.
- Prefer small, deterministic tests that do not require hardware.
- Mock the `hal::Can` interface for protocol-level tests.

6) Build system tips
- Presets are the primary interface: see `CMakePresets.json`.
- Standalone builds fetch `embedded-infra-lib` automatically via FetchContent.
- When consumed as a subdirectory by another project, `embedded-infra-lib` must already be available.
- `compile_commands.json` is generated in build dirs; use it for language server/analysis.

7) When making changes, be explicit
- Update `documents/spec/can-protocol.md` and `documents/requirements/can-protocol.yaml` when changing protocol behavior.
- **Boundary rule.** A category belongs in can-lite **if and only if** it concerns the node as a protocol participant or as a device, and is agnostic to what the device does. A category that ascribes meaning to the payload in application terms belongs to the consumer. System (0x0) and Firmware Upgrade (0x1) qualify; motor control, sensing and actuation do not. New application-specific categories go in consumer projects, never here.

8) Document consistency (must check before finishing any change)
- The following documents must stay aligned with each other and with the code:
  - `README.md` — project overview, quick-start, feature list.
  - `documents/spec/can-protocol.md` — wire-format specification (CAN ID layout, message types, encoding).
  - `documents/requirements/can-protocol.yaml` — formal requirements (traceability to spec and tests).
  - `documents/design/architecture.md` — architecture decisions, patterns, component relationships.
- After any protocol, structural, or behavioral change, review all four documents for consistency. Update any document that contradicts or omits the new behavior.
- Pay particular attention to: CAN ID bit-field definitions, category IDs, message type values, observer interfaces, and the category hierarchy description.

9) Quick pointers for reviewers / code suggestions
- If suggesting new APIs, prefer interface-driven DI and small, testable functions.
- Observer callbacks must not allocate or block.
