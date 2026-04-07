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
- **Interfaces / abstract classes**: use the plain class name — no `I` prefix. Example: `IsoTpTransport` (not `IIsoTpTransport`)
- **Concrete implementations**: use the plain name with `Impl` suffix. Example: `IsoTpTransportImpl` (not `IsoTpTransport`)
- **No-heap instantiation via `WithStorage`**: concrete `Impl` classes define a nested `WithStorage` template alias following the EMIL pattern. The key rules:
  1. The `Impl` class is **not** templated on storage sizes — sizes live only in the `WithStorage` alias.
  2. The `Impl` constructor takes a **reference to the EMIL container** (e.g. `infra::BoundedVector<T>&`) as its first argument — not a custom `Storage` struct.
  3. `infra::WithStorage<Base, StorageType>` privately owns the storage and passes it as the first constructor argument to `Base`.
  4. Use `infra::BoundedVector<T>::WithMaxSize<N>` as the storage type for pool-style containers.

  **Simple example** (pool of items):
  ```cpp
  class FooImpl : public Foo
  {
  public:
      template<std::size_t MaxItems>
      using WithStorage = infra::WithStorage<FooImpl,
          typename infra::BoundedVector<Item>::template WithMaxSize<MaxItems>>;

      explicit FooImpl(infra::BoundedVector<Item>& items, Bar& bar);
  private:
      infra::BoundedVector<Item>& items_;
  };
  // Usage: FooImpl::WithStorage<8> foo{ bar };
  ```
  **Non-template class with templated element storage** (all sizes in WithStorage alias):
  ```cpp
  class TransportImpl : public Transport
  {
  public:
      template<uint16_t MaxPduSize, uint8_t MaxChannels = 4>
      using WithStorage = infra::WithStorage<TransportImpl,
          typename infra::BoundedVector<Channel<MaxPduSize>>::template WithMaxSize<MaxChannels>>;

      template<typename ChannelType>
      explicit TransportImpl(infra::BoundedVector<ChannelType>& channels, FrameTransport& ft);
  };
  // Usage: TransportImpl::WithStorage<64, 4> tp{ frameTransport };
  ```

## Style

- Allman brace style, 4-space indent
- Functions ≤ 30 lines (hard limit 50)
- `const` on all non-mutating methods
- `constexpr` for compile-time calculations
- Fixed-size integer types: `uint8_t`, `int32_t`, etc.
- Prefer `{}` (brace) initialization over `()` for all variables and member data: `uint8_t count{}`, `MyClass obj{arg1, arg2}`
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
- No pure virtual destructors unless strictly necessary — they add vtable entries and increase binary/RAM size; prefer a non-pure virtual destructor or omit the destructor entirely when the class is not deleted polymorphically through a base pointer
- Observer callbacks must not allocate or block
