# Migration Guide

## 0.x → 1.0.0

Release 1.0.0 draws a boundary around what can-lite owns. A category belongs in
this library **if and only if** it concerns the node as a protocol participant or
as a device, and is agnostic to what the device does. A category that ascribes
meaning to the payload in application terms belongs to the consumer.

That rule removes the FOC motor category and promotes consumer-owned categories
to first-class citizens. The wire format is otherwise unchanged: the 29-bit
identifier still carries priority at bit 24 (5 bits), category at bit 20
(4 bits), message type at bit 12 (8 bits) and node ID at bit 0 (12 bits).

### 1. `can_lite.categories.foc_motor` is gone

`FocMotorCategoryServer`, `FocMotorCategoryClient`, their observers, their
definitions header, their unit tests, `documents/spec/foc-motor-control.md`,
`documents/requirements/foc-motor-control.yaml` and
`integration_tests/features/foc_motor_category.feature` have all been deleted.
The category has **not** been moved to another repository, and there is no build
option that brings it back.

**What to do:** move the motor category into the project that owns the motor.
It becomes an ordinary consumer category, built exactly like the echo category in
`can-lite/testing/`:

- take the category ID as a constructor parameter instead of a hard-coded
  `constexpr` — the integrator owns that assignment now;
- derive privately and first from `CanCategoryHandlerStorage<Max>`;
- bind handlers with `AddMessageType(messageTypeId, handler)`;
- send through `Outbound()`;
- register from your composition root with `RegisterCategory`.

A migrated category links `can_lite.core` only.

### 2. Category IDs now have a policy

| Range     | Owner                                              |
|-----------|----------------------------------------------------|
| `0x0`–`0x1` | can-lite management categories (System, Firmware Upgrade) |
| `0x2`–`0x7` | Integrator-assigned application categories       |
| `0x8`–`0xF` | Reserved                                         |

A node supports at most **8** registered categories, which is exactly what one
category-discovery response frame holds. `RegisterCategory` now rejects an ID
outside `0x0`–`0x7`, a duplicate ID, and a registration that would exceed the
maximum count — on the client as well as the server. In exchange,
`SendCategoryList` no longer silently truncates its response.

**What to do:** if you assigned a category ID in `0x8`–`0xF`, move it into
`0x2`–`0x7`. If you registered more than 8 categories, consolidate them.

### 3. The acknowledgement payload is a fixed 5 bytes

| Byte | Field             |
|------|-------------------|
| 0    | Category          |
| 1    | Message type      |
| 2    | Status            |
| 3    | Correlation       |
| 4    | Expected sequence |

The size and the meaning of every field are now identical for every status.
`correlation` echoes the sequence number of the request being acknowledged;
`expectedSequence` is only meaningful when the status is `sequenceError`.

Correlation is a core concern, so per-category correlation conventions are
retired — including the FOC `0x80 + command` convention and the firmware
upgrader's private scheme. Decode acknowledgements with `CanCommandAck` /
`canCommandAckSize` from `CanProtocolDefinitions.hpp`.

**What to do:** update any code that parsed a 3-byte acknowledgement or that
derived correlation from the message type.

### 4. `CanAckStatus::categoryError` is removed

Value `7` is withdrawn and left unassigned; values `0`–`6` keep their current
numbering so no renumbering is needed. Per-category error taxonomies are the
consumer's problem — encode them in a category response, not in the shared
acknowledgement status.

Unknown message types continue to answer `unknownCommand`; `notImplemented`
remains for a message type that is recognised but not implemented.

### 5. Sequence handling

- Sequence state is per **(peer, category)** on both sides, held in bounded
  fixed arrays. Previously the client counted per node and the server kept a
  single global counter.
- A sequence mismatch used to be permanent: the server did not advance its
  counter while the sender already had, so one lost frame bricked the link. The
  `sequenceError` acknowledgement now carries the expected sequence and the
  client resynchronises automatically.
- When the peer table fills, the oldest entry is reused. A busy bus no longer
  aborts the node.
- `CanCategoryClient` no longer exposes `PeekSequence` / `CommitSequence`;
  sequence allocation lives in the outbound handle.

### 6. Categories no longer see the transport or the protocol host

`CanProtocolServer::RegisterCategory` / `CanProtocolClient::RegisterCategory`
create a per-category `CanCategoryOutbound` handle bound to that category's ID,
attach it for the lifetime of the registration, and detach it on unregistration.
The handle owns sequence allocation and carries the acknowledger, so
`CanCommandAcknowledger` and `SetAcknowledger` are gone along with the
null-check assert they required.

**What to do:** drop `CanFrameTransport&` and `CanProtocolClient&` /
`CanProtocolServer&` constructor parameters from your categories and send
through `Outbound()`. The firmware upgrade categories changed the same way.

### 7. One byte-range entry point per message type

`CanMessageType.hpp` is deleted. There is no longer a nested class per message
type, and no separate `Handle(const hal::Can::Message&)` and
`HandlePdu(infra::ConstByteRange)`. A category owns a bounded array of
`(messageTypeId, infra::Function<bool(infra::ConstByteRange)>)` bindings, added
with `AddMessageType`.

This is a behaviour change as well as a refactor. Previously a raw frame always
counted as handled once the identifier matched, because `Handle` returned
`void`, while the same malformed payload arriving over ISO-TP could be rejected.
Both paths now use the PDU semantics: the handler returns `false` to reject, the
host answers `invalidPayload`, and an unrecognised message type answers
`unknownCommand`. The `really_assert(false)` default that aborted the node on an
unopted segmented frame is gone.

`RequiresSequenceValidation()` is pure virtual, so each category declares its own
policy instead of inheriting `true` from `CanCategoryServer` or `false` from
`CanCategoryClient`.

### 8. New target: `can_lite.testing`

The generic echo category (`EchoCategoryServer` / `EchoCategoryClient`) and
`VirtualCan` ship in `can-lite/testing/`. The echo category is the reference
example for writing a consumer category and exercises registration, dispatch,
sequencing, rate limiting, discovery, ISO-TP and acknowledgement. Link
`can_lite.testing` in host tests; it is not needed on target.
