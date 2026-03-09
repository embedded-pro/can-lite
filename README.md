[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=embedded-pro_can-lite&metric=alert_status&token=2d1b7ae361d044a96ba29c5afcbdb009cac319d2)](https://sonarcloud.io/summary/new_code?id=embedded-pro_can-lite)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=embedded-pro_can-lite&metric=coverage&token=2d1b7ae361d044a96ba29c5afcbdb009cac319d2)](https://sonarcloud.io/summary/new_code?id=embedded-pro_can-lite)
[![Duplicated Lines (%)](https://sonarcloud.io/api/project_badges/measure?project=embedded-pro_can-lite&metric=duplicated_lines_density&token=2d1b7ae361d044a96ba29c5afcbdb009cac319d2)](https://sonarcloud.io/summary/new_code?id=embedded-pro_can-lite)
[![Vulnerabilities](https://sonarcloud.io/api/project_badges/measure?project=embedded-pro_can-lite&metric=vulnerabilities&token=2d1b7ae361d044a96ba29c5afcbdb009cac319d2)](https://sonarcloud.io/summary/new_code?id=embedded-pro_can-lite)

# can-lite

A lightweight, extensible CAN bus protocol library implementing a client-server model over CAN 2.0B (29-bit extended identifiers). Designed for deterministic, low-latency communication on resource-constrained embedded systems with no heap allocation.

## Overview

can-lite provides a minimal CAN protocol framework with a clear client-server architecture:

- **Server** — Listens for commands from clients, processes them via category-based dispatch, and sends acknowledgement responses. Each server has a unique 12-bit node ID.
- **Client** — Initiates all requests and queries. A single client can communicate with multiple servers on the same CAN bus.

The protocol ships with a built-in **System** category (heartbeat, command acknowledgement, status request). Applications extend it by registering custom category handlers for domain-specific messages.

## Features

### Protocol
- **Client-Server Architecture**: Clear separation of roles — client initiates, server responds
- **Multi-Server Support**: One client can address multiple servers via node IDs
- **Category-Based Dispatch**: Extensible message routing via pluggable category handlers (server/client pairs)
- **Built-in System Category**: Heartbeat, command acknowledgement, status request, and category discovery out of the box
- **Sequence Validation**: 8-bit sequence counter with per-category opt-in
- **Rate Limiting**: Configurable message rate enforcement on the server
- **Fixed-Point Codec**: Saturation-clamped encoding for float-to-integer conversion

### Embedded Constraints
- **Zero Heap Allocation**: All memory statically allocated at compile time
- **Fixed Memory Footprint**: Uses `infra::BoundedVector`, `infra::BoundedDeque`, `infra::Function` from embedded-infra-lib
- **Deterministic Execution**: No dynamic allocation or unbounded loops
- **CAN 2.0B Only**: 29-bit extended identifiers; 11-bit frames silently discarded

## Getting Started

### Prerequisites
- CMake 3.24 or later
- C++20 compatible compiler
- [embedded-infra-lib](https://github.com/embedded-pro/embedded-infra-lib) (fetched automatically when building standalone)

### Quick Start

1. Clone the repository
```bash
git clone --recursive https://github.com/embedded-pro/can-lite.git
cd can-lite
```

2. Configure and build
```bash
cmake --preset host
cmake --build --preset host-Debug
```

3. Run tests
```bash
ctest --preset host-Debug
```

### Using as a Library

Add can-lite as a subdirectory or FetchContent dependency in your CMake project:

```cmake
FetchContent_Declare(
    can-lite
    GIT_REPOSITORY https://github.com/embedded-pro/can-lite.git
    GIT_TAG main
)
FetchContent_MakeAvailable(can-lite)

target_link_libraries(your_target PRIVATE can_lite.core can_lite.server can_lite.client)
```

## Project Structure

```
├── can-lite/
│   ├── core/                   # Protocol definitions, frame codec, transport
│   │   ├── CanProtocolDefinitions.hpp  # Enums, constants, CAN ID layout
│   │   ├── CanFrameCodec.hpp           # Fixed-point encoding/decoding
│   │   ├── CanFrameTransport.hpp       # Async frame queue and send
│   │   ├── CanCategory.hpp             # Base category hierarchy (Server/Client)
│   │   ├── CanMessageType.hpp          # Message handler interface
│   │   └── test/                       # Core unit tests
│   ├── categories/             # Category implementations (server/client pairs)
│   │   ├── system/             # Built-in System category (heartbeat, ack, discovery)
│   │   └── foc_motor/          # FOC Motor Control category (extension example)
│   ├── server/                 # Server implementation
│   │   ├── CanProtocolServer.hpp       # Server with dispatch & observer
│   │   └── test/                       # Server unit tests
│   ├── client/                 # Client implementation
│   │   ├── CanProtocolClient.hpp       # Client with discovery & observer
│   │   └── test/                       # Client unit tests
│   └── drivers/                # Hardware driver adapters
├── documents/
│   ├── design/                 # Architecture & design decisions
│   ├── requirements/           # Protocol requirements (YAML)
│   └── spec/                   # Protocol specification (Markdown)
└── CMakeLists.txt
```

## Key Design Principles

### Memory Management
- **No Heap Allocation**: All memory is statically allocated at compile time
- **Bounded Containers**: Uses `infra::BoundedVector`, `infra::BoundedDeque` instead of STL containers
- **Fixed Memory Footprint**: Predictable RAM usage for resource-constrained systems

### Architecture
- **Dependency Injection**: `hal::Can` and category handlers injected via constructor
- **Observer Pattern**: Decouples protocol events from application logic using `infra::Subject` / `infra::SingleObserver`
- **Interface-Driven Design**: Pure virtual interfaces (`CanProtocolServer`, `CanProtocolClient`) enable mocking and testing
- **Type-Safe Categories**: Compile-time separation via `CanCategoryServer` / `CanCategoryClient` prevents cross-registration
- **Category Extensibility**: Register custom category implementations (server/client pairs) for application-specific messages

### Safety
- **Deterministic Execution**: No dynamic allocation or unbounded loops in message paths
- **Input Validation**: All payloads length-checked before parsing
- **Rate Limiting**: Configurable per-server message rate enforcement
- **Sequence Validation**: Per-category opt-in replay protection

## Documentation

- [Architecture & Design](documents/design/architecture.md) — Architecture decisions and design patterns
- [Protocol Specification](documents/spec/can-protocol.md) — Full wire-format specification
- [FOC Motor Control Specification](documents/spec/foc-motor-control.md) — FOC Motor Control category specification
- [Firmware Upgrade Specification](documents/spec/firmware-upgrade.md) — Firmware Upgrade category specification
- [Protocol Requirements](documents/requirements/can-protocol.yaml) — Formal requirements
- [Copilot Instructions](.github/copilot-instructions.md) — Development guidelines

## Contributing

### Development Workflow
1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Follow the coding standards in [copilot-instructions.md](.github/copilot-instructions.md)
4. Ensure all tests pass
5. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
6. Push to the branch (`git push origin feature/AmazingFeature`)
7. Open a Pull Request

### Critical Guidelines
- **NO heap allocation** (`new`, `delete`, `malloc`, `free`)
- **NO standard containers** (use `infra::Bounded*` alternatives)
- Use fixed-size types (`uint8_t`, `int32_t`, etc.)
- Mark all non-mutating methods as `const`
- Write unit tests for new functionality
- Follow `.clang-format` rules for code formatting

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
