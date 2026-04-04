---
description: "Use when implementing or designing CAN bus protocol categories based on industry standards like UDS (ISO 14229), J1939 (SAE), ISO-TP (ISO 15765-2), or CANopen (CiA 301/402). Provides protocol-specific encoding rules, message formats, timing parameters, and guidance on mapping each standard to the can-lite category model."
---

# CAN Bus Protocol Standards Reference

Use this reference when implementing can-lite categories that map to or interact with industry CAN bus standards.

## UDS — Unified Diagnostic Services (ISO 14229)

### Service IDs

| SID  | Service                    | Positive Response |
|------|----------------------------|-------------------|
| 0x10 | DiagnosticSessionControl   | 0x50              |
| 0x11 | ECUReset                   | 0x51              |
| 0x14 | ClearDiagnosticInformation | 0x54              |
| 0x19 | ReadDTCInformation         | 0x59              |
| 0x22 | ReadDataByIdentifier       | 0x62              |
| 0x27 | SecurityAccess             | 0x67              |
| 0x2E | WriteDataByIdentifier      | 0x6E              |
| 0x31 | RoutineControl             | 0x71              |
| 0x34 | RequestDownload            | 0x74              |
| 0x36 | TransferData               | 0x76              |
| 0x37 | RequestTransferExit        | 0x77              |
| 0x3E | TesterPresent              | 0x7E              |

### Negative Response Codes (NRC)

| NRC  | Name                                    |
|------|-----------------------------------------|
| 0x10 | generalReject                           |
| 0x11 | serviceNotSupported                     |
| 0x12 | subFunctionNotSupported                 |
| 0x13 | incorrectMessageLengthOrInvalidFormat   |
| 0x22 | conditionsNotCorrect                    |
| 0x24 | requestSequenceError                    |
| 0x31 | requestOutOfRange                       |
| 0x33 | securityAccessDenied                    |
| 0x35 | invalidKey                              |
| 0x72 | generalProgrammingFailure               |
| 0x78 | requestCorrectlyReceivedResponsePending |

### Key Rules

- Positive response SID = request SID + 0x40
- Negative response: 0x7F + rejected SID + NRC
- Subfunction bit 7 = suppress positive response
- DID (Data Identifier): 2 bytes, big-endian
- Sessions: default (0x01), programming (0x02), extended diagnostic (0x03)
- Timing: P2 (default response, ~50ms), P2* (extended after NRC 0x78, ~5000ms)

### Mapping to can-lite

- Each UDS SID maps to a `CanMessageType` within a UDS category
- Command SIDs (0x10–0x3E) → command message types (0x00–0x7F range)
- Positive response SIDs (0x50–0x7E) → response message types (0x80–0xFF range)
- Negative response → dedicated message type with payload [rejected_SID, NRC]
- UDS messages > 8 bytes require ISO-TP segmentation below the category layer
- SecurityAccess uses seed-challenge: requestSeed (subfunction odd) → sendKey (subfunction even)

---

## J1939 — SAE Heavy-Duty Vehicle Network

### CAN ID Layout (29-bit)

```
[28:26] Priority (3 bits)
[25]    Reserved (1 bit, set to 0)
[24]    Data Page (1 bit)
[23:16] PDU Format / PF (8 bits)
[15:8]  PDU Specific / PS (8 bits)
[7:0]   Source Address (8 bits)
```

- **PDU1** (PF < 240): PS = Destination Address (peer-to-peer)
- **PDU2** (PF ≥ 240): PS = Group Extension (broadcast)
- **PGN** = `(DP << 16) | (PF << 8) | PS` (PDU2) or `(DP << 16) | (PF << 8)` (PDU1)

### Key PGNs

| PGN    | Name            | Description                    |
|--------|-----------------|--------------------------------|
| 0x0000 | TSC1            | Torque/Speed Control           |
| 0xEA00 | Request         | Request specific PGN           |
| 0xEE00 | Address Claimed | Address management             |
| 0xFECA | DM1             | Active DTCs                    |
| 0xFECB | DM2             | Previously Active DTCs         |
| 0xFEF1 | CCVS            | Cruise Control/Vehicle Speed   |
| 0xF004 | EEC1            | Electronic Engine Controller 1 |

### Transport Protocol (messages > 8 bytes)

**BAM (Broadcast Announce Message) — unacknowledged:**
1. TP.CM_BAM (PGN 0xEC00): control byte=0x20, total size (2B LE), num packets, 0xFF, PGN (3B LE)
2. TP.DT (PGN 0xEB00): sequence number (1B, starts at 1), data (7B), pad with 0xFF

**CMDT (Connection Mode Data Transfer) — acknowledged:**
1. RTS (0x10): total size, num packets, max packets per CTS, PGN
2. CTS (0x11): num packets to send, next sequence number, PGN
3. DT: sequence number, data (7B)
4. EOM (0x13): total size, num packets, PGN
5. Abort (0xFF): reason code, PGN

### Mapping to can-lite

- J1939 uses a fundamentally different CAN ID layout than can-lite
- Options: (a) J1939 gateway category that translates PGN addressing to can-lite's category/message-type model, (b) raw J1939 pass-through at the driver layer
- PGN-to-category mapping is application-specific
- Source Address (8-bit) is narrower than can-lite's Node ID (12-bit)
- Multi-packet via BAM/CMDT must be handled at the transport layer

---

## ISO-TP — Transport Protocol (ISO 15765-2)

### Frame Types

| Type                   | PCI Byte(s) | Description                               |
|------------------------|-------------|-------------------------------------------|
| SF (Single Frame)      | `0x0N`      | N = data length (1–7)                     |
| FF (First Frame)       | `0x1NNN`    | NNN = total length (8–4095), 2 PCI bytes  |
| CF (Consecutive Frame) | `0x2N`      | N = sequence number (0–F, wraps)          |
| FC (Flow Control)      | `0x3S`      | S = flow status, followed by BS and STmin |

### Flow Control Parameters

- **FS (Flow Status)**: 0 = ContinueToSend, 1 = Wait, 2 = Overflow/Abort
- **BS (Block Size)**: 0 = no flow control (send all CFs), N = send N CFs then wait for FC
- **STmin (Separation Time Minimum)**:
  - 0x00–0x7F: 0–127 milliseconds
  - 0x80–0xF0: reserved
  - 0xF1–0xF9: 100–900 microseconds
  - 0xFA–0xFF: reserved

### Segmentation Rules

- **Sender**: SF if ≤ 7 bytes; FF + CFs if > 7 bytes
- **Receiver**: Send FC after receiving FF, then after every BS consecutive frames
- **Sequence number**: Starts at 1 in first CF after FF, wraps 0→F→0
- **Padding**: Unused bytes in last CF padded with 0xCC or 0x00 (configurable)

### Timing Parameters

| Parameter | Description               | Typical Value     |
|-----------|---------------------------|-------------------|
| N_As      | Sender transmit time      | 1000ms max        |
| N_Ar      | Receiver transmit time    | 1000ms max        |
| N_Bs      | Time between FF and FC    | 1000ms max        |
| N_Br      | Time between CF and FC    | —                 |
| N_Cs      | Time between CFs (sender) | STmin + tolerance |
| N_Cr      | CF reception timeout      | 1000ms max        |

### Mapping to can-lite

- ISO-TP sits between the application layer (UDS, etc.) and CAN
- Preferred: Implement as a core transport component (companion to `CanFrameTransport`) that handles segmentation/reassembly transparently
- Alternative: Implement as a category, but this requires upper-layer categories to interact with it
- The segmentation layer must manage FC timeouts via `infra::TimerSingleShot` (non-blocking, event-driven)
- Buffer sizes must be bounded (`infra::BoundedVector::WithMaxSize<N>`) — determine max message size at compile time

---

## CANopen — Industrial Automation (CiA 301/402)

### COB-ID Structure (11-bit standard)

| Function Code | COB-ID Range   | Description        |
|---------------|----------------|--------------------|
| NMT           | 0x000          | Network Management |
| SYNC          | 0x080          | Synchronization    |
| EMCY          | 0x080 + NodeID | Emergency          |
| TPDO1         | 0x180 + NodeID | Transmit PDO 1     |
| RPDO1         | 0x200 + NodeID | Receive PDO 1      |
| TPDO2         | 0x280 + NodeID | Transmit PDO 2     |
| RPDO2         | 0x300 + NodeID | Receive PDO 2      |
| TSDO          | 0x580 + NodeID | SDO Server→Client  |
| RSDO          | 0x600 + NodeID | SDO Client→Server  |
| Heartbeat     | 0x700 + NodeID | NMT Error Control  |

### NMT State Machine

States: Initializing → Pre-operational ↔ Operational ↔ Stopped

| CS   | Command                           |
|------|-----------------------------------|
| 0x01 | Start Remote Node (→ Operational) |
| 0x02 | Stop Remote Node (→ Stopped)      |
| 0x80 | Enter Pre-operational             |
| 0x81 | Reset Node                        |
| 0x82 | Reset Communication               |

NMT command frame: COB-ID 0x000, payload = [CS, Node-ID] (Node-ID 0 = all nodes)

### SDO Protocol

**Expedited download (≤ 4 data bytes):**
- Client→Server (RSDO): `[CCS=1 | n<<2 | e=1 | s=1, index_lo, index_hi, subindex, data0, data1, data2, data3]`
- Server→Client (TSDO): `[SCS=3, index_lo, index_hi, subindex, 0, 0, 0, 0]`

**Segmented download (> 4 data bytes):**
1. Initiate: CCS=1, e=0, s=1, index, subindex, size (4 bytes)
2. Segments: CCS=0, toggle bit alternating, data (7 bytes)
3. Last segment: CCS=0, toggle, n=(7-remaining), c=1, data

**Abort**: CCS=4, index, subindex, abort code (4 bytes LE)

### Heartbeat

COB-ID 0x700 + NodeID, 1-byte payload:
- 0x00: Boot-up
- 0x04: Stopped
- 0x05: Operational
- 0x7F: Pre-operational

### Object Dictionary

- Entries addressed by Index (16-bit) : Sub-Index (8-bit)
- 0x1000–0x1FFF: Communication profile
- 0x2000–0x5FFF: Manufacturer-specific
- 0x6000–0x9FFF: Standardized device profile

### Mapping to can-lite

- CANopen natively uses 11-bit standard CAN IDs; can-lite uses 29-bit extended IDs
- When bridging CANopen over can-lite, map: function code → category (4-bit), and embed node-ID in can-lite's node address field
- SDO and PDO services each map naturally to a can-lite category
- NMT can map to the System category or a dedicated NMT category
- Heartbeat consumer/producer maps well to can-lite's existing heartbeat pattern
- Object dictionary access (SDO) requires careful state management — use `std::optional<T>` for pending transfers, bounded buffers for segmented data
