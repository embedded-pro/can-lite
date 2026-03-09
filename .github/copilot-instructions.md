# can-lite — Copilot / AI agent instructions

This file is a concise, task-oriented guide for AI coding agents to be immediately productive in this repository.

1) Big-picture architecture (short)
- Purpose: Lightweight, extensible CAN bus protocol library implementing a client-server model over CAN 2.0B (29-bit extended identifiers). Designed for embedded systems with strict memory and timing constraints.
- Major components:
  - `can-lite/core/` — Protocol definitions (enums, CAN ID layout, constants), frame codec (fixed-point encoding), frame transport (async send queue), and the base `CanCategory` hierarchy (`CanCategoryServer` / `CanCategoryClient`).
  - `can-lite/categories/` — Category implementations split into server/client pairs. Built-in: `system/` (heartbeat, ack, discovery). Extension example: `foc_motor/`.
  - `can-lite/server/` — Server implementation: listens for commands, dispatches to category handlers, sends acknowledgements. Uses observer pattern for application callbacks.
  - `can-lite/client/` — Client implementation: sends commands/queries to servers, receives responses. Supports multiple servers via node addressing.
  - `can-lite/drivers/` — Hardware driver adapters.
  - `embedded-infra-lib/` — Infrastructure dependency: bounded containers, build helpers, `hal::Can`, `infra::Subject`/`infra::SingleObserver`.
- Architecture: Client initiates all requests; Server listens and responds. Built-in System category (0x0) provides heartbeat, ack, status request, and category discovery. Categories are split into server/client pairs inheriting from `CanCategoryServer`/`CanCategoryClient` for compile-time type safety. All category handlers use `infra::Subject`/`infra::SingleObserver` for event notification. Applications extend via custom category implementations.
- Documents: `documents/spec/can-protocol.md` (wire-format spec), `documents/requirements/can-protocol.yaml` (formal requirements), `documents/design/architecture.md` (architecture & design decisions), `README.md` (project overview).

2) Critical developer workflows (exact commands)
- Clone:
  - `git clone --recursive <repo>`
- Configure & build host (recommended first step):
  - `cmake --preset host`
  - `cmake --build --preset host-Debug`
- Run unit tests (GoogleTest):
  - `ctest --preset host-Debug`
- Coverage/analysis presets are defined in `CMakePresets.json` — use `coverage` preset for coverage builds.

3) Project-specific constraints and conventions (must follow these)
- NO HEAP: avoid `new/delete`, `malloc/free`, `std::make_unique`, etc.
- NO dynamic STL containers: use `infra::BoundedVector`, `infra::BoundedDeque`, `infra::BoundedString`, `infra::Function`, etc. (see `embedded-infra-lib`).
- Prefer fixed-size integer types (`uint8_t`, `int32_t`, ...).
- Favor `constexpr`, `inline`, and `const` correctness.
- All multi-byte values on the wire are big-endian.

4) Patterns & code locations (concrete examples)
- Add a new message category:
  - Define a new `CanCategory` enum value in `can-lite/core/CanProtocolDefinitions.hpp`.
  - Implement a `CanCategoryServer` subclass and/or a `CanCategoryClient` subclass in `can-lite/categories/<name>/`.
  - Register the handler with `CanProtocolServer::RegisterCategory()` (server side) or `CanProtocolClient::RegisterCategory()` (client side).
- Add a new message type to an existing category:
  - Add the `CanMessageType` enum value in `CanProtocolDefinitions.hpp`.
  - Create a `CanMessageType` subclass and register it via `AddMessageType()` in the category constructor.
  - Handle it in the subclass's `Handle()` method and notify via the category's observer.
- Fixed-point encoding: use `CanFrameCodec` helpers (`FloatToFixed16`, `Fixed16ToFloat`, `WriteInt16`, `ReadInt16`, etc.).
- Observer pattern: all categories expose events through `infra::Subject<Observer>` / `infra::SingleObserver`. Server-level events use `infra::Subject<CanProtocolServerObserver>`. Category-level events use per-category observer interfaces (e.g. `FocMotorCategoryServerObserver`).

5) Testing & CI expectations
- Unit tests run on host using GoogleTest.
- Tests are in `can-lite/core/test/`, `can-lite/server/test/`, `can-lite/client/test/`.
- Prefer small, deterministic tests that do not require hardware.
- Mock the `hal::Can` interface for protocol-level tests.

6) Build system tips
- Presets are the primary interface: see `CMakePresets.json`.
- Standalone builds fetch `embedded-infra-lib` automatically via FetchContent.
- When consumed as a subdirectory by another project, `embedded-infra-lib` must already be available.
- `compile_commands.json` is generated in build dirs; use it for language server/analysis.

7) When making changes, be explicit
- Update `documents/spec/can-protocol.md` and `documents/requirements/can-protocol.yaml` when changing protocol behavior.
- Keep the System category (0x0) as the only built-in category; application-specific categories belong in consumer projects.

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
