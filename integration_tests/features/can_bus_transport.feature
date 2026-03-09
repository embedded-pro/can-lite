Feature: CAN Bus Transport
  The protocol uses CAN 2.0B (29-bit extended identifiers) for all
  communication. Both client and server must discard 11-bit standard frames.

  Background:
    Given a CAN bus with a server at node 1 and rate limit 500
    And a CAN bus client connected to the same bus

  # REQ-CAN-002: Extended CAN identifiers
  Scenario: Server discards 11-bit standard identifier frames
    When a frame is received with an 11-bit standard identifier
    Then the server shall silently discard the frame

  Scenario: Client discards 11-bit standard identifier frames
    When the client receives a frame with an 11-bit standard identifier
    Then the client shall silently discard the frame

  # REQ-CAN-003: CAN identifier structure
  Scenario: CAN identifier encodes priority, category, message type, and node ID
    When a CAN identifier is constructed with priority 4, category 2, message type 1, and node ID 42
    Then the 29-bit identifier shall encode priority in bits 28-24
    And the 29-bit identifier shall encode category in bits 23-20
    And the 29-bit identifier shall encode message type in bits 19-12
    And the 29-bit identifier shall encode node ID in bits 11-0
