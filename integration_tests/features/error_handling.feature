Feature: Error Handling
  The protocol defines specific error handling behaviors including
  node address filtering, unknown categories, and payload validation.

  Background:
    Given a CAN bus with a server at node 1 and rate limit 500
    And a CAN bus client connected to the same bus

  @REQ-CAN-005
  Scenario: Server ignores frames addressed to a different node
    When a frame is sent addressed to node 99
    Then the server shall silently discard the frame

  @REQ-CAN-007
  Scenario: Server accepts broadcast messages (node 0)
    When a heartbeat frame is sent addressed to the broadcast address 0
    Then the server shall process the broadcast heartbeat

  @REQ-CAN-020
  Scenario: Unknown category is silently discarded
    When a frame is received with unregistered category 15
    Then the server shall silently discard the frame

  @REQ-CAN-021
  Scenario: Unknown message type in registered category triggers error
    When a frame is received with system category and unknown message type 126
    Then the server shall send an acknowledgement with status "unknownCommand"

  @REQ-CAN-021
  Scenario: Response-range message type is silently discarded rather than acked as unknown
    When a frame is received with system category and unknown message type 255
    Then the server shall silently discard the frame

  @REQ-CAN-016
  Scenario: Short payload on a custom category command is rejected
    Given the demo category is registered on the server
    And a sequenced test category with ID 4 is registered on the server
    When the client sends a set parameters command with only 3 bytes
    Then the demo server observer shall not have received a set parameters command

  Scenario: Client silently ignores messages from unregistered categories
    When the client receives a response for unregistered category 14
    Then the client shall silently discard the frame
