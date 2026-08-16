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
    When a frame is received with system category and unknown message type 255
    Then the server shall send an acknowledgement with status "unknownCommand"

  @REQ-CAN-016
  Scenario: A payload shorter than the message type requires is rejected
    Given a sequenced test category with ID 3 is registered on the server
    When the client sends a command to category 3 with a payload shorter than the handler requires
    Then the server category handler shall not have accepted the command

  Scenario: Client silently ignores messages from unregistered categories
    When the client receives a response for unregistered category 14
    Then the client shall silently discard the frame
