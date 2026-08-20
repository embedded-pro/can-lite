# Glossary

Terms, abbreviations and the constants they map to. Chapter references point at
the fullest treatment of each term.

## Protocol vocabulary

| Term | Meaning |
|------|---------|
| **Broadcast node ID** | Node address `0x000` (`canBroadcastNodeId`). Accepted by every server; used by the client's heartbeat, since a client has no address of its own. Chapter 8 |
| **Category** | A functional group of messages sharing a 4-bit category ID, implemented as a server/client class pair. The unit of extension. Chapters 6, 12 |
| **Category error** | A category-specific failure reported as message type `0xFE`, carrying the originating command ID and a category-defined error code. Chapter 6 |
| **Client** | The node that initiates commands. One per bus in the supported topology; has no node ID of its own. Chapter 8 |
| **Command** | A client → server message; message types `0x00`–`0x7F`. Chapter 5 |
| **Command acknowledgement** | System-category message `0x02`: four bytes carrying category, message type, `CanAckStatus` and the expected sequence number. Chapter 7 |
| **Heartbeat** | System-category message `0x01`, one payload byte (the protocol version), emitted after a quiet period rather than periodically. Chapters 7, 8, 10 |
| **Message type** | The 8-bit field identifying a message within its category. Commands `0x00`–`0x7F`, responses `0x80`–`0xFF`, `0xFE` reserved. Chapter 5 |
| **Node ID** | The 12-bit address field. Destination on a command, source on a response. Chapters 5, 7 |
| **PDU** | Protocol Data Unit: a payload longer than one frame, segmented by ISO-TP. Chapter 9 |
| **Priority** | The 5-bit field in bits 28:24; lower wins arbitration. `emergency` 0, `command` 4, `response` 8, `telemetry` 12, `heartbeat` 16. Chapter 5 |
| **Quiet period / silence guard** | The heartbeat rule: a single-shot timer restarted after every outgoing frame, so a heartbeat appears only after real silence. Chapter 7 |
| **Response** | A server → client message; message types `0x80`–`0xFF`. Chapter 5 |
| **Sequence number** | An 8-bit replay/ordering counter in `data[0]` of a validated command; the server accepts `(previous + 1) mod 256`. Chapters 6, 7 |
| **Server** | A node that answers commands and emits telemetry and heartbeats; has a unique 12-bit node ID and serves exactly one client. Chapter 7 |
| **Telemetry** | Server → client data sent at `CanPriority::telemetry`, unsolicited and unacknowledged. Chapter 6 |

## ISO-TP vocabulary

| Term | Meaning |
|------|---------|
| **BS** | Block Size: how many consecutive frames the sender may send before waiting for another flow-control frame. can-lite's receiver always requests `0` (no interruption). Chapter 9 |
| **CF** | Consecutive Frame, PCI `0x2`: up to 7 further payload bytes plus a 4-bit sequence number. Chapter 9 |
| **Channel** | A `(dataId, fcId)` identifier pair with one sender and one receiver. Chapter 9 |
| **FC** | Flow Control frame, PCI `0x3`: flow status, block size, STmin. Chapter 9 |
| **FF** | First Frame, PCI `0x1`: 12-bit total length plus the first 6 payload bytes. Chapter 9 |
| **Flow status** | `0` continue to send, `1` wait, `2` overflow. Chapter 9 |
| **N_Bs** | Sender's timeout waiting for flow control; 1000 ms in can-lite. Chapter 9 |
| **N_Cr** | Receiver's timeout waiting for the next consecutive frame; 1000 ms. Chapter 9 |
| **N_WFTmax** | Maximum consecutive FC `wait` frames a sender tolerates; 16 (`nWftMax`). Chapter 9 |
| **PCI** | Protocol Control Information: the first nibble of byte 0 identifying the frame type. Chapter 9 |
| **SF** | Single Frame, PCI `0x0`: a whole PDU of 1–7 bytes. Chapter 9 |
| **STmin** | Separation time between consecutive frames: `0x00`–`0x7F` = 0–127 ms, `0xF1`–`0xF9` = 100–900 µs, anything else 127 ms. Chapter 9 |

## Implementation vocabulary

| Term | Meaning |
|------|---------|
| **Bounded container** | An `infra::Bounded*` type with a compile-time maximum, used everywhere `std::vector`/`std::string`/`std::deque` would be. Chapter 3 |
| **EMIL** | [embedded-infra-lib](https://github.com/embedded-pro/embedded-infra-lib): the source of `infra::*` and `hal::*`. Chapter 3 |
| **Event dispatcher** | `infra::EventDispatcher`, which runs scheduled work in the single application context. Chapter 3 |
| **`infra::Function`** | Fixed-capacity, non-allocating replacement for `std::function`. Chapter 3 |
| **Intrusive list** | `infra::IntrusiveList<T>`: links stored inside the element, so registration allocates nothing — and so server and client categories are type-incompatible. Chapter 3 |
| **Observer / Subject** | `infra::SingleObserver<Observer, Subject>` and `infra::Subject<Observer>`: one observer per subject, attached in its constructor and detached in its destructor. Chapter 3 |
| **`really_assert`** | EMIL's always-on assertion, retained in release builds; used only for programming errors. Chapter 3 |
| **Sticky validity** | The `CanPayloadReader`/`CanPayloadWriter` rule: once an operation goes out of range, `Valid()` stays `false` and later operations are no-ops. Chapter 5 |
| **`WithStorage`** | The EMIL pattern in which an `Impl` class is non-template and takes a reference to its storage, while a nested `WithStorage<N>` alias owns that storage. Chapter 3 |

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `canProtocolVersion` | 1 | Heartbeat payload byte |
| `canBroadcastNodeId` | `0x000` | Broadcast address |
| `canSystemCategoryId` | `0x00` | System category |
| `canFirmwareUpgradeCategoryId` | `0x01` | Firmware upgrade category |
| `canFirstApplicationCategoryId` … `canLastApplicationCategoryId` | `0x02` … `0x0F` | Application category range |
| `canMaxRegisteredCategories` | 8 | Categories per protocol object, system included |
| `canLastCommandMessageTypeId` | `0x7F` | Command/response boundary |
| `canCategoryErrorResponseMessageTypeId` | `0xFE` | Category error, in every category |
| `canCommandAckSize` | 4 | Acknowledgement payload bytes |
| `canHeartbeatMessageTypeId` | `0x01` | System heartbeat |
| `canCommandAckMessageTypeId` | `0x02` | System acknowledgement |
| `canStatusRequestMessageTypeId` | `0x03` | System status request |
| `canCategoryListRequestMessageTypeId` | `0x04` | Discovery request |
| `canCategoryListResponseMessageTypeId` | `0x05` | Discovery response |
| `CanProtocolClient::maxServers` | 8 | Tracked servers, for sequence and for liveness |
| Send queue depth | 8 | Frames queued behind an in-flight send |
| `nBsTimeout`, `nCrTimeout` | 1000 ms | ISO-TP timeouts |
| `nWftMax` | 16 | ISO-TP consecutive FC wait frames |
| `maxSupportedChannels` | 16 | Upper bound on ISO-TP channels |
| `sfMaxPayloadBytes` | 7 | Single-frame payload capacity |
| `ffFirstDataBytes` | 6 | First-frame payload capacity |
| `cfMaxDataBytes` | 7 | Consecutive-frame payload capacity |

## Status and error enumerations

| `CanAckStatus` | Value | Meaning |
|----------------|-------|---------|
| `success` | 0 | Command accepted and handled |
| `unknownCommand` | 1 | No handler for that message type in that category |
| `invalidPayload` | 2 | Payload missing or too short |
| `invalidState` | 3 | Command not valid in the current state |
| `sequenceError` | 4 | Sequence mismatch; byte 3 carries the expected value |
| `rateLimited` | 5 | Defined but never emitted — over-limit traffic is dropped silently |
| `notImplemented` | 6 | Recognised but unsupported |
| `categoryError` | 7 | Detail is in a separate `0xFE` frame |

| `FwuState` | Value | | `FwuError` | Value |
|---|---|---|---|---|
| `idle` | 0 | | `ok` | 0 |
| `receiving` | 1 | | `busy` | 1 |
| `verifying` | 2 | | `invalidSize` | 2 |
| `complete` | 3 | | `sequenceError` | 3 |
| `error` | 4 | | `writeError` | 4 |
| | | | `crcMismatch` | 5 |
| | | | `notReady` | 6 |
| | | | `invalidState` | 7 |
| | | | `sessionTimeout` | 8 |

| `iso_tp::AbortReason` | Raised when |
|-----------------------|-------------|
| `nBsTimeout` | No flow control within N_Bs |
| `nCrTimeout` | No consecutive frame within N_Cr |
| `overflow` | Peer signalled overflow, or an incoming PDU exceeds the buffer |
| `unexpectedFrame` | Unparseable frame for the current state, or a failed raw send |
| `waitLimitExceeded` | More than `nWftMax` consecutive FC wait frames |
