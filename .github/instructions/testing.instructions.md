---
description: "can-lite testing guidelines: GoogleTest/GoogleMock patterns, StrictMock usage, test file naming, no heap allocation in tests, Arrange-Act-Assert pattern, mock hal::Can, cucumber-cpp-runner integration tests."
applyTo:
  - "**/test/**"
  - "**/integration_tests/**"
---

# can-lite Testing Guidelines

## Framework

- **Unit tests**: GoogleTest + GoogleMock
- **Integration tests**: cucumber-cpp-runner (BDD / Gherkin) in `integration_tests/`

## File Structure

- Unit test files: `can-lite/{module}/test/Test{ComponentName}.cpp`
- Test doubles: `can-lite/{module}/test/` or `can-lite/{module}/test_doubles/`
- Integration features: `integration_tests/features/*.feature`
- Integration steps: `integration_tests/steps/*.cpp`

## Unit Test Pattern

```cpp
#include "can-lite/path/Component.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(ComponentTest, specific_behavior_description)
{
    // Arrange
    // Act
    // Assert
}
```

## Rules

- **ONLY `testing::StrictMock<>` is allowed** — `NiceMock`, `NaggyMock`, and bare mock classes are **forbidden**; every unexpected call must cause immediate test failure
- Mock `hal::Can` interface for protocol-level tests
- No heap allocation in tests — same memory constraints as production code
- Test edge cases: empty payload, max payload (8 bytes), boundary values, invalid inputs
- Test sequence error paths for server categories
- Verify observer notifications in tests
- Each new `CanMessageType` must have at least one test

## Test-Driven Development (TDD)

- Write tests **before** production code: Red → Green → Refactor
- Requirements must be **clear and unambiguous** before writing tests — if anything is uncertain, ask the user to clarify before proceeding
- Each test must correspond to a specific requirement from `documents/requirements/`
- Failing tests define the acceptance criteria; implementation is only done to make those tests pass

## Integration Tests

- `ApplicationFixture` composes connected `VirtualCan` pairs, `CanProtocolServer`, `CanProtocolClient`, and mock observers
- `VirtualCan` simulates a CAN bus — frames sent by one instance are delivered to the connected instance
- Use `ForwardTime()` to advance timers (heartbeat, rate limiting)
- `testing::StrictMock<>` on all observers — every interaction must be explicitly expected; no `NiceMock` or `NaggyMock` exceptions
- Avoid capturing `shared_ptr` to fixture in step lambdas (prevents circular references)

## Build & Run

```bash
cmake --build --preset host-Debug
ctest --preset host-Debug
```
