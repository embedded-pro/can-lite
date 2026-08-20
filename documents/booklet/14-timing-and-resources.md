# Timing, Memory and Bus Budget

Everything can-lite consumes is fixed at compile time or configured at
construction. This chapter collects the numbers: which timers exist, what the
tunable parameters mean, how much static RAM each component costs, and how much
of the bus a given traffic pattern uses.

## 1. Timer inventory

| Owner | Timer | Type | Armed / restarted by | On expiry |
|-------|-------|------|----------------------|-----------|
| `CanProtocolServer` | `heartbeatTimer` | single-shot | every outgoing frame from this node | send a heartbeat |
| `CanProtocolServer` | `rateResetTimer` | **repeating**, 1 s | construction, then itself | reset the accepted-frame counter |
| `CanProtocolServer` | `clientLivenessTimer` | single-shot | every frame addressed to this node | notify `Offline()` |
| `CanProtocolClient` | `heartbeatTimer` | single-shot | every outgoing frame from this node | broadcast a heartbeat |
| `CanProtocolClient` | `serverLiveness[i].timeoutTimer` | single-shot × 8 | every frame from that server | notify `OnServerOffline(node)` |
| `CanProtocolClient` | `serverStates[i].ackTimer` | single-shot × 8 | `CommitSequence` for that server | notify `OnCommandAckTimeout(...)` |
| `FirmwareUpgradeCategoryServer` | `sessionTimeoutTimer` | single-shot | Begin Upgrade and every Data Block | notify `OnSessionTimeout()` |
| `IsoTpSender` | `nBsTimer` | single-shot, 1000 ms | FF sent, block-final CF sent, FC `wait` received | `abort(nBsTimeout)` |
| `IsoTpSender` | `stMinTimer` | single-shot, `STmin` | after each CF, when `STmin` > 0 | send the next CF |
| `IsoTpReceiver` | `nCrTimer` | single-shot, 1000 ms | FF accepted, each CF accepted | `abort(nCrTimeout)` |

A fully loaded client — eight servers tracked, eight commands outstanding —
holds **17** live timers. A server holds three, plus one per firmware upgrade
category and two per ISO-TP channel. All of them are members; none is allocated.

Only one timer in the entire library is periodic. An idle node with no ISO-TP
channels open has exactly two timers armed on the server side (heartbeat, rate
reset) and one on the client side (heartbeat) — everything else is dormant until
something happens.

## 2. Tunable parameters

| Parameter | Where | Default | Meaning |
|-----------|-------|---------|---------|
| `nodeId` | `CanProtocolServer::Config` | 0 (invalid) | 12-bit server address; `0x000` asserts |
| `maxMessagesPerSecond` | `CanProtocolServer::Config` | 500 | Accepted frames per tumbling 1 s window |
| `heartbeatInterval` | both `Config`s | 1 s | Silence after which a heartbeat is emitted |
| `clientTimeout` | `CanProtocolServer::Config` | 3 s | Silence after which the client is declared offline |
| `serverTimeout` | `CanProtocolClient::Config` | 3 s | Silence after which a server is declared offline |
| `commandAckTimeout` | `CanProtocolClient::Config` | 1 s | Wait for the acknowledgement of a sequence-validated command |
| `sessionTimeout` | `FirmwareUpgradeCategoryServer::Config` | 30 s | Inactivity after which an upgrade session is abandoned |

And the ones that are **not** tunable, because they are compile-time constants:

| Constant | Value | Where |
|----------|-------|-------|
| `canMaxRegisteredCategories` | 8 | `CanProtocolDefinitions.hpp` |
| `CanProtocolClient::maxServers` | 8 | `CanProtocolClient.hpp` |
| Send queue depth | 8 | `CanFrameTransport.hpp` |
| `nBsTimeout`, `nCrTimeout` | 1000 ms | `IsoTpTypes.hpp` |
| `nWftMax` | 16 | `IsoTpTypes.hpp` |
| `maxSupportedChannels` | 16 | `IsoTpTransportImpl.hpp` |

### Choosing them

**Liveness timeout versus heartbeat interval.** A timeout must survive at least
two lost heartbeats, or an idle bus will flap between online and offline:

```text
timeout ≥ 3 × heartbeatInterval        (the defaults: 3 s ≥ 3 × 1 s)
```

**Acknowledgement timeout versus handler latency.** `commandAckTimeout` must
exceed the worst-case time from frame reception to acknowledgement — including
any asynchronous work the observer defers (a flash write, an ADC conversion) —
plus the queueing delay of a busy bus. A flash page write of 20 ms behind a
saturated send queue is still comfortably inside the 1 s default; a handler that
waits for a mechanical actuator is not.

**Rate limit versus command rate.** The limit must exceed the client's
steady-state frame rate toward that server, including heartbeats and any
telemetry acknowledgements. Because the window is tumbling (Chapter 13, §7.2),
budget the *peak* at half the configured limit if bursts matter.

**Session timeout versus block cadence.** It must exceed the longest gap the
client can legitimately leave between blocks — typically dominated by the
client's own flash read or network fetch, not by the bus.

## 3. Static memory

can-lite allocates nothing, so its footprint is the sum of its members. The
structural counts are fixed; the per-element sizes depend on the toolchain's
`infra::Function` capacity and alignment, so the honest way to get exact numbers
is to ask the compiler:

```cpp
static_assert(sizeof(services::CanProtocolServer) < 1024, "server footprint");

printf("server  %zu\n", sizeof(services::CanProtocolServer));
printf("client  %zu\n", sizeof(services::CanProtocolClient));
printf("isotp   %zu\n", sizeof(services::IsoTpTransportImpl::WithStorage<1024, 4>));
```

What the structure guarantees, independent of toolchain:

| Component | Fixed-count storage |
|-----------|---------------------|
| `CanFrameTransport` | 8 × { 4-byte ID, 8-byte message + length, one `infra::Function` } + 2 `Function` slots |
| `CanProtocolServer` | 1 transport, 3 timers, the system category, ~6 bytes of counters and flags |
| `CanProtocolClient` | 1 transport, 1 timer, the system category, 8 × `PerServerState` (≈ 6 bytes + a timer each), 8 × `ServerLiveness` (≈ 3 bytes + a timer each) |
| `IsoTpSender` / `IsoTpReceiver` | `MaxPduSize` bytes of buffer, 2 or 1 timers, ≈ 10 bytes of state |
| `IsoTpChannelImpl` | one sender + one receiver + 3 `Function` slots + 9 bytes |
| `IsoTpTransportImpl` | `MaxChannels` channels + a 16-pointer array + 2 `Function` slots |
| Each category | Its own members plus one `CanMessageHandler` (≈ 3 pointers) per message type |

The dominant term is almost always ISO-TP:

```text
ISO-TP bytes ≈ MaxChannels × (2 × MaxPduSize + channel overhead)
```

`WithStorage<1024, 4>` is therefore about **8 KiB** of buffers, and
`WithStorage<128, 2>` about **512 bytes**. Sizing this is the single most
consequential memory decision an integrator makes; the transport rejects PDUs
larger than `MaxPduSize` at both ends (Chapter 13, §8.1), so it is a hard
ceiling rather than a performance knob.

Stack usage is bounded by the deepest call chain, which is receive → ISO-TP →
category → observer → send. Nothing recurses except the send-queue drain, and
only if a driver violates the asynchronous-completion assumption (Chapter 13,
§1.7).

## 4. Bus arithmetic

An extended (29-bit) CAN frame costs 67 bits of framing plus 8 bits per data
byte, before bit stuffing. Worst-case stuffing adds up to about a quarter of the
stuffable field.

| Frame | Data bytes | Nominal bits | Worst case with stuffing |
|-------|-----------|--------------|--------------------------|
| Heartbeat | 1 | 75 | ≈ 91 |
| Acknowledgement | 4 | 99 | ≈ 121 |
| Typical command | 7 | 123 | ≈ 151 |
| Full frame | 8 | 131 | ≈ 160 |

Frame time at common bitrates (nominal / worst case, 8 data bytes):

| Bitrate | Bit time | 8-byte frame |
|---------|----------|--------------|
| 125 kbit/s | 8 µs | 1.05 ms / 1.28 ms |
| 250 kbit/s | 4 µs | 524 µs / 640 µs |
| 500 kbit/s | 2 µs | 262 µs / 320 µs |
| 1 Mbit/s | 1 µs | 131 µs / 160 µs |

### What the rate limit means in bus load

At 500 kbit/s, the default limit of 500 frames per second toward one server is

```text
500 × 262 µs ≈ 131 ms per second ≈ 13 % bus load
```

which leaves ample room for other servers' responses and telemetry. At
125 kbit/s the same limit is over 50 % of the bus, and is almost certainly too
high: the limiter protects the *server's* processing budget, but the bus is the
scarcer resource at low bitrates.

### What a command exchange costs

A sequence-validated command with a response is three frames — command,
response, acknowledgement — so at 500 kbit/s a full request/response cycle
occupies roughly 750 µs of bus time. A control loop issuing 100 set-points per
second to each of four servers therefore uses about 30 % of the bus in frames
alone.

### Firmware upgrade throughput

Per data block: one command frame (8 bytes), one Data Block Ack (3 bytes) and
one acknowledgement (4 bytes) — about 321 nominal bits, carrying 6 payload
bytes.

| Bound | 500 kbit/s | 1 Mbit/s |
|-------|-----------|----------|
| Line rate only | ≈ 9.3 KiB/s | ≈ 18.7 KiB/s |
| With a 1 ms round trip (stop-and-wait) | ≈ 5.4 KiB/s | ≈ 5.7 KiB/s |

The second row is the one that matters. Because the protocol is stop-and-wait —
the client waits for each block's acknowledgement before sending the next — the
**round-trip latency**, not the bitrate, sets the throughput. Making the bus
faster barely helps; making the exchange deeper does. That is why the three
extensions listed in Chapter 11, §8 (windowed acknowledgement, larger blocks via
ISO-TP, page addressing) all target the number of round trips rather than the
frame time.

A 256 KiB image at 5.4 KiB/s takes about 48 seconds. Comfortably inside a
service window; not something to do at every boot.

### ISO-TP transfer time

For a PDU of *n* bytes with `STmin = 0` and `BS = 0`:

```text
frames  = 1 + ceil((n - 6) / 7)                 (FF + CFs)
bus time ≈ frames × frameTime + 1 × FC frameTime
```

A 1024-byte PDU is 1 FF + 146 CFs = 147 frames, about 39 ms at 500 kbit/s —
roughly forty times more efficient per payload byte than the same data sent as
firmware blocks, because there is no per-block turnaround.

## 5. Processing cost

| Operation | Cost |
|-----------|------|
| Identifier decode | Four shifts and masks, all `constexpr`-capable |
| Category lookup | Linear scan over ≤ 8 entries |
| Message-type lookup | Linear scan over the category's handlers (typically 2–6) |
| Sequence validation | One increment and one comparison |
| Rate limiting | One comparison and one increment |
| Payload access | Bounds check plus byte moves; no allocation |
| ISO-TP channel lookup | Linear scan over ≤ 16 channels, two comparisons each |

The receive path is therefore a few dozen instructions plus the handler's own
work. On a 100 MHz Cortex-M, the fixed part is well under a microsecond, which
is one to two orders of magnitude below the frame time even at 1 Mbit/s — the
bus, not the CPU, is the constraint.

The one caveat is that all of this runs **inside the driver's receive
callback**. A handler that does real work there extends the time before the next
frame can be processed. That is the mechanical reason behind the rule of
Chapter 3: observers do not block, and long work is deferred with a completion
function.

## 6. A worked budget

A four-axis machine: one controller (client) and four drives (servers), 500
kbit/s, each drive receiving 200 set-points per second and returning telemetry
at 100 Hz.

| Traffic | Frames/s | Bits/s | Bus load |
|---------|----------|--------|----------|
| Set-points (7 bytes) | 4 × 200 = 800 | 800 × 123 = 98 400 | 19.7 % |
| Acknowledgements (4 bytes) | 800 | 800 × 99 = 79 200 | 15.8 % |
| Telemetry (8 bytes) | 4 × 100 = 400 | 400 × 131 = 52 400 | 10.5 % |
| Heartbeats | ≈ 0 | ≈ 0 | ≈ 0 % — the bus is never quiet |
| **Total** | **2000** | **230 000** | **46 %** |

Observations worth carrying into a real design:

- **Acknowledgements are not free.** They cost nearly as much bus time as the
  commands themselves. A category whose commands do not need per-command
  acknowledgement (like firmware upgrade, which has its own responses) saves a
  third of its traffic.
- **The rate limit must be raised.** Each drive receives 200 set-points per
  second plus its own share of nothing else — inside the 500 default — but a
  configuration with 600 set-points per second per drive would be silently
  clipped.
- **46 % is a reasonable target.** CAN degrades gracefully but latency for
  low-priority frames grows quickly above roughly 60–70 % load, and the
  telemetry priority (12) is below command (4) and response (8) precisely so
  that it yields first.
- **Heartbeats disappear on a busy bus.** The silence-guard design means the
  heartbeat contributes nothing to a loaded bus and appears only when the
  machine is idle, which is exactly when there is room for it.
