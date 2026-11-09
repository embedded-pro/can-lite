# The Built-in Categories: Design Rationale

can-lite ships two categories. Their messages, payload layouts, states and error
codes are specified in the
[Protocol Specification](../spec/can-protocol.md) §8 (system) and the
[Firmware Upgrade Specification](../spec/firmware-upgrade.md) (upgrade), and
the system category's role inside the protocol objects is described in
[Architecture and Design Decisions](../design/architecture.md) §6. This chapter
answers the questions those documents do not: **why each is built the way it
is, and what that costs.**

## 1. The system category is a category

Nothing forced the protocol-level conversations — presence, acknowledgement,
status, discovery — to travel the same road as application messages. They could
have been special-cased inside the protocol objects.

Making them an ordinary category buys three things:

- **One dispatch path.** A received frame reaches its handler by exactly one
  route, whatever it is for.
- **One set of rules.** The system category occupies a registration slot, uses
  the same payload handling and the same observer pattern as any other. That is
  a useful forcing function: a rule that is awkward for the system category is
  probably awkward for everyone.
- **Nothing special to learn.** A reader who understands one category
  understands this one.

What it costs is one of the eight registration slots, permanently.

### Why the halves are not mirror images

The two halves handle disjoint sets of messages, and the asymmetry is
informative rather than accidental:

| Conversation    | Server half                    | Client half                   |
|-----------------|--------------------------------|-------------------------------|
| Presence        | handles the client's broadcast | —                             |
| Acknowledgement | sends them                     | recognises them, acts nowhere |
| Status request  | handles it                     | —                             |
| Discovery       | answers it                     | consumes the answer           |

A server's heartbeat arriving at a client finds no handler, and nothing goes
wrong: the client already extracted that frame's value — proof of life — before
dispatch (Chapter 7), and clients never acknowledge, so an unhandled message
type on the client side is genuinely inert.

The acknowledgement and category list handlers on the client half are
deliberately empty. The work happens in the protocol object, which runs first
because it needs the source identity that dispatch does not carry. The handler exists so that the category's
registered message types remain an accurate description of what it understands —
a small piece of hygiene, not dead code.

### What the application does and does not see

The server exposes **nothing** of the system category: everything it does is
already reflected in the server's own interface — online and offline
notifications, automatic answers to status and discovery. Exposing it would let
an application answer a status request differently from the protocol's
definition.

The client exposes **only discovery**, because it is the one answer an
application must interpret.

Two consequences follow, and both are simplifications rather than oversights:

- **Acknowledgement statuses do not reach the application.** A client learns a
  command's fate from the category's own response, from the acknowledgement
  timeout, or — for a sequence error — not at all, because the counter
  resynchronises silently. An application that needs to distinguish, say,
  "invalid state" from "not implemented" must carry that in its own category's
  response, which is what the shipped categories do.
- **The heartbeat's protocol version is not checked.** It is on the wire for
  future negotiation; today two firmware generations with different versions
  will talk to each other regardless. Worth knowing before deploying a mixed
  bus (Chapter 10, §6.7).

## 2. The firmware upgrade category keeps no state

This is the library's fullest worked example of a category whose state lives in
the **application**. The category owns message framing, one timer and response
encoding; the flash layout, the buffering, the checksum and the bank switch
belong to the product.

```mermaid
flowchart LR
    subgraph cat["The category owns"]
        C1["message framing"]
        C2["the session timer"]
        C3["response and acknowledgement encoding"]
    end
    subgraph app["The application owns"]
        A1["storage layout and staging"]
        A2["size and concurrency policy"]
        A3["block ordering expectations"]
        A4["page buffering and writing"]
        A5["verification, including authenticity"]
        A6["activation and rollback"]
    end
    cat -- "notifies, with a completion" --> app
    app -- "completes, with a status" --> cat
```

The division is what makes the category reusable across products with completely
different storage. It is also what makes the application's obligations
non-negotiable, listed in §4 below.

### Why it does not validate sequence numbers

Two reasons, both practical:

1. **The block index already orders the transfer.** A duplicated or reordered
   block is detected by the application against its own expectation and answered
   with a category error that says which block was expected — which a
   protocol-level sequence error could not.
2. **A sequence number would cost a payload byte in the message that sends the
   most bytes.** One byte out of seven, on the longest transfer the protocol
   ever performs, is a sixth of the throughput.

The trade-off is real: this category has no protocol-level duplicate detection.
It is bounded instead by the checksum at the end — a repeated or lost block
produces a mismatch and the image is not activated — and by the authenticity
check the application is expected to add.

### Why polling does not extend the session

The session timer is started by a begin the application accepts, extended by
each block it accepts, and stopped by the commands that end the transfer as they
arrive. Asking for progress does **neither**.

The timer follows the application's answer, not the command's arrival. A begin
that is refused opens no session, so it cannot expire half a minute later, and a
block that fails to write does not buy the client more time.

That is the design decision worth remembering: a client that has crashed
mid-transfer, but whose supervisor still polls for progress, must not be able to
hold the server's staging area open indefinitely.

When the timer does expire the category notifies the application and does
nothing else — no frame is sent, because there is nobody to tell, and no state
is changed, because the state that matters belongs to the application.

### The asynchronous completion pattern

Flash operations take milliseconds to seconds, and nothing in can-lite may block
for that long. Every notification therefore carries a completion for the
application to call when it is done, and the category retains nothing across the
gap — everything it needs to compose the answer is captured when it notifies.

```mermaid
sequenceDiagram
    participant Cat as Firmware upgrade category
    participant App as Application observer
    participant Flash as Storage driver

    Cat->>App: a data block arrived, here is a completion
    App->>App: copy into the page buffer
    alt page not yet full
        App-->>Cat: completion, success — immediately
    else page full
        App->>Flash: write the page
        Note over App,Cat: the handler returns — no frame sent yet
        Flash-->>App: write finished
        App-->>Cat: completion, with the outcome
    end
    Cat-->>Cat: response and acknowledgement composed and sent
```

Three obligations fall on the application, and none can be checked:

- **Call the completion exactly once.** Never calling it leaves the client
  waiting for its own response timeout — the commands carry no sequence number,
  so the protocol layer runs no acknowledgement timeout for them. Calling it
  twice sends two answers to one command.
- **Copy a block before returning.** Its bytes are lent for the notification
  only; they belong to the received frame, which is gone once it returns.
- **The category does not serialise.** A second command arriving before the
  first completion has run will be dispatched. An application that cannot cope
  must refuse the overlapping command itself.

A completion may still run after the category has been unregistered — a flash
write cannot be recalled. Its response is sent; its acknowledgement is dropped,
because the category no longer has a server to speak through.

### Why it answers when no application is attached

A category notification with no observer is normally discarded (Chapter 10,
5.12). Here that would leave the client with nothing: its commands carry no
sequence number, so no acknowledgement timeout would fire either. The category
therefore answers every command itself while no application is attached — not
ready, or an idle progress report — as the
[Firmware Upgrade Specification](../spec/firmware-upgrade.md) sets out in §4.8.

### Why every command produces two frames back

A category response and a protocol acknowledgement answer different questions:
the acknowledgement says the command was well-formed and reached a handler; the
response says what happened. When a handler fails, the acknowledgement reports a
category-level failure and the detail travels in the response — which is exactly
the split described in Chapter 5, §2.

The cost is bus traffic: acknowledgements are nearly as expensive as the
commands they answer (Chapter 11, §6). A category that carries its own status,
as this one does, is the case where switching them off would save a third of the
traffic — a change the protocol does not currently allow, and a fair candidate
for a future extension.

## 3. Throughput is bounded by round trips, not by the bus

The transfer is stop-and-wait: each block waits for its acknowledgement before
the next is sent. The consequence is that **round-trip latency, not bitrate,
sets the throughput** — making the bus twice as fast barely helps.

That is why all three extensions recorded in the specification — windowed
acknowledgement, page addressing, and carrying blocks over segmentation
(Chapter 8) — attack the number of round trips rather than the frame time. The
arithmetic is in Chapter 11, §4.

## 4. What the application must provide

| Concern            | What the category expects                                         |
|--------------------|-------------------------------------------------------------------|
| Storage layout     | Somewhere to stage an image that is not the running one           |
| Size policy        | Refuse an image that does not fit                                 |
| Concurrency policy | Refuse a second session while one is open                         |
| Block ordering     | Compare each index with the expected one and report a gap         |
| Buffering          | Accumulate small blocks into whatever unit the storage writes     |
| Verification       | Check the image against the client's checksum                     |
| Activation         | Switch to the new image, ideally with rollback if it fails to run |
| Timeout recovery   | Discard staging state when the session expires                    |
| **Authenticity**   | **Verify a signature, if the product needs one**                  |

The last row is the one to read twice. can-lite authenticates nothing: any node
that can put frames on the bus can start an upgrade. Where that matters, the
image must carry its own signature and the application must check it before
reporting success.

## 5. The upgrade session: one application, ready made

The category keeps no state so that any storage can sit behind it. Most products
built on the infrastructure library stage the same thing, though: a signed
upgrade pack, written to a flash slot, checked, and handed to the bootloader
through its status journal. `FirmwareUpgradeSession` is that application,
shipped in its own library, `can_lite.categories.firmware_upgrade.session`, so
that a product need not write it again. A product with other storage ignores it
and observes the category itself, exactly as §2 describes.

### What it owns, and what it asks for

| Port                     | Provided by | What the session does with it                                                                              |
|--------------------------|-------------|------------------------------------------------------------------------------------------------------------|
| Firmware staging         | the product | The pack slot, the journal, the product name and application capacity a pack must match, the page size     |
| Firmware upgrade hold    | the product | Asks whether the application is stopped and idle, holds it while the session works, releases it afterwards |
| Reset                    | the product | Restarts the node into the bootloader once a pack is activated                                             |
| Firmware upgrade control | the session | Lets the product ask whether the session is running, which version is staged, and cancel a download        |

Staging is optional. A node constructed without it refuses every begin as too
large and every activation as not ready, so one image can run on boards with and
without a staging slot.

The session meets the obligations of §4 as follows: the slot is erased whole
before the begin is answered, blocks are buffered into pages and each page is
read back after it is programmed, the client's checksum is compared over the
size given in the begin, and the pack's own header — product, target, version
and size — is checked against the staging limits before the pack counts as
staged. Authenticity is left to the bootloader, which verifies the pack's
signature before it installs it.

### The session's states

The states reported to the client (the
[Firmware Upgrade Specification](../spec/firmware-upgrade.md) §2) are a coarser
view of these.

```mermaid
stateDiagram-v2
    [*] --> Idle
    [*] --> Staged : journal reads downloaded at start-up
    Idle --> Erasing : begin accepted
    Staged --> Erasing : begin accepted
    Error --> Erasing : begin accepted
    Erasing --> Receiving : slot erased
    Erasing --> Error : erase failed
    Receiving --> Verifying : verify with every byte received
    Receiving --> Error : page write failed
    Verifying --> Staged : checksum and header pass
    Verifying --> Error : checksum or header fails
    Erasing --> Idle : abort or cancel
    Receiving --> Idle : abort, cancel or timeout
    Verifying --> Idle : abort or cancel
    Staged --> Idle : abort discards the pack
    Error --> Idle : abort
    Staged --> Restarting : activate
    Restarting --> [*] : reset after the restart delay
```

| Session state         | Reported to the client | Application held |
|-----------------------|------------------------|------------------|
| Idle                  | idle                   | no               |
| Erasing and Receiving | receiving              | yes              |
| Verifying             | verifying              | yes              |
| Staged and Restarting | complete               | Restarting only  |
| Error                 | error                  | no               |

### The decisions worth knowing

- **Nothing starts while the application runs.** A begin or an activation is
  refused busy unless the hold reports the application stopped and idle, and
  from the first erase until the pack is staged, fails or is abandoned the
  application stays held. Activation keeps it held until the reset.
- **The journal is the only memory.** A staged pack is recorded as downloaded in
  the bootloader's status journal, so it survives a restart: the session reads
  the staged header back at start-up and reports the pack complete. Activation
  records it ready to deploy, answers, and resets after a short delay so the
  answer leaves the node first.
- **A flash step is never cut short.** An abort, a cancel or an expiry that
  arrives while a sector is erased or a page programmed waits for that step to
  finish. The command it interrupted is answered invalid state first, the abort
  after it.
- **The journal is written only while the application is stopped or held.** The
  journal is written synchronously. Aborting a staged pack therefore forgets it
  at once, but clears the journal only once the application is stopped and idle,
  retrying until it is.
- **Two deadlines, one outcome.** The session keeps its own inactivity deadline
  beside the category's session timer (§2); whichever fires first ends a download
  that is receiving blocks. Neither ends an erase or a verification.
- **Cancel is for the product.** Firmware upgrade control's cancel ends a
  download the way an abort does, without an abort answer, and leaves a staged
  pack and an accepted activation alone — what a product needs when its own
  emergency handling must stop the transfer.
