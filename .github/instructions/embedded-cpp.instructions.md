---
description: "Embedded C++ coding rules for can-lite: no heap allocation, bounded containers, event-driven non-blocking model, Allman brace style, PascalCase naming, SOLID principles, const correctness, CAN 2.0B wire format conventions."
applyTo: "**/*.{hpp,cpp,h}"
---

# can-lite Embedded C++ Rules

## Memory — No Heap Allocation

**FORBIDDEN:**
- `new`, `delete`, `malloc`, `free`
- `std::make_unique`, `std::make_shared`
- `std::vector`, `std::string`, `std::deque`, `std::list`, `std::map`, `std::set`

**REQUIRED replacements:**
- `infra::BoundedVector<T>::WithMaxSize<N>` instead of `std::vector<T>`
- `infra::BoundedString::WithStorage<N>` instead of `std::string`
- `infra::BoundedDeque<T>::WithMaxSize<N>` instead of `std::deque<T>`
- `infra::IntrusiveList<T>` for linked lists
- `std::optional<T>` for values that may be absent
- `std::array<T, N>` for fixed-size arrays
- Stack and static allocation only

## Naming

- **Classes/Methods**: `PascalCase` — `CanCategoryServer`, `HandleMessage()`
- **Member variables/Enum values**: `camelCase` — `nodeId`, `heartbeat`
- **Namespaces**: lowercase — `services` (match the current can-lite codebase)

## Style

- Allman brace style, 4-space indent
- Functions ≤ 30 lines (hard limit 50)
- `const` on all non-mutating methods
- `constexpr` for compile-time calculations
- Fixed-size integer types: `uint8_t`, `int32_t`, etc.
- No exceptions — use `std::optional<T>` or status enums

## CAN Wire Format

- All multi-byte values **big-endian** on the wire
- Use `CanFrameCodec` helpers for encoding/decoding — not manual bit shifts
- CAN ID layout (29-bit extended): `(priority << 24) | (category << 20) | (message_type << 12) | node_id`
- Maximum 8 bytes per CAN 2.0 frame
- 29-bit extended IDs only; 11-bit standard IDs are not used

## Category Pattern

- Server categories inherit `CanCategoryServer`, client categories inherit `CanCategoryClient`
- Observer interfaces via `infra::Subject<Observer>` / `infra::SingleObserver`
- Message types registered via `AddMessageType()` in the category constructor
- Command message types: 0x00–0x7F, response message types: 0x80–0xFF
- Server categories require sequence validation by default; client categories do not

## Design Principles

- Single Responsibility: one class = one concern
- Dependency Injection: all dependencies via constructor
- DRY: never duplicate logic
- Self-documenting code through clear naming — no comments restating code
- Observer callbacks must not allocate or block
