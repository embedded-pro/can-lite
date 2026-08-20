# Glossary

Protocol vocabulary — node, category, command, response, priority, sequence
number, PDU and the rest — is defined in the
[Protocol Specification](../spec/can-protocol.md) §2, and the constants and
enumerations are tabulated in §5, §6 and §10 of the same document. Segmentation
frame types and timing parameters are in §4.1.

This glossary covers only the vocabulary this booklet adds: the implementation
patterns the design is built from, and the terms used to classify behaviour.

## Design and implementation vocabulary

| Term                         | Meaning                                                                                                                                                                                                                        |
|------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Binding**                  | The association of one message identifier with one member function of the category that handles it. The unit of dispatch inside a category (Chapter 5)                                                                         |
| **Bounded container**        | A container with a compile-time maximum, used everywhere a growing container would be. Reaching the maximum is a defined outcome, never an allocation                                                                          |
| **Channel**                  | A pair of identifiers — one for data, one for the flow control that answers it — with one sender and one receiver (Chapter 8)                                                                                                  |
| **Completion**               | A function handed to an observer so it can report the outcome later, without the notifying component holding state across the gap (Chapter 9)                                                                                  |
| **Design limit**             | A deliberate simplification whose cost is stated rather than hidden. Used as a marker throughout Chapter 10                                                                                                                    |
| **EMIL**                     | [embedded-infra-lib](https://github.com/embedded-pro/embedded-infra-lib), the infrastructure library supplying the bounded containers, timers, callbacks and observer pattern                                                  |
| **Eviction**                 | Reclaiming the least recently claimed slot of a full fixed-size table, in rotation, rather than refusing the newcomer (Chapter 7)                                                                                              |
| **Latent**                   | Behaviour that is correct today but depends on an assumption a driver or an integrator could break. Used as a marker throughout Chapter 10                                                                                     |
| **Peek and commit**          | Taking the next sequence number and advancing the counter as two steps, so a refused send does not consume a number (Chapter 7)                                                                                                |
| **Reference, not ownership** | The composition rule: components borrow their collaborators, and the application owns everything (Chapter 2)                                                                                                                   |
| **Silence guard**            | A timer restarted by activity, so the action it guards happens only after real inactivity. How both heartbeats work (Chapter 6)                                                                                                |
| **Sticky validity**          | Payload composition and consumption poison themselves on the first out-of-range access, so a caller checks once rather than per field (Chapter 4)                                                                              |
| **Tumbling window**          | A counting window that resets wholesale on a timer, as opposed to a sliding one. Why a burst can cross a boundary at twice the limit (Chapter 10, §7.2)                                                                        |
| **`WithStorage`**            | The pattern in which an implementation is non-template and borrows its storage, while a nested alias owns that storage and carries the size. Described in [Architecture and Design Decisions](../design/architecture.md) §10.1 |

## Roles

| Term                | Meaning                                                                                                                                                                                                                        |
|---------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Acknowledger**    | The role, played by the server protocol object, through which a category has an acknowledgement sent on its behalf — because an acknowledgement belongs to the system category, and no category speaks for another (Chapter 5) |
| **Frame transport** | The single outbound queue per node, owned by the protocol object and lent to every category (Chapter 4)                                                                                                                        |
| **Sequence source** | The role, played by the client protocol object, through which a client category obtains sequence numbers without depending on the client library (Chapter 5)                                                                   |
| **Observer**        | The application-side implementation of a category's or protocol object's event interface. Exactly one per subject, bound for its own lifetime                                                                                  |
