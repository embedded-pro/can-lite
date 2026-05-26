Feature: System Category
  The built-in System category (0x0) provides heartbeat, command
  acknowledgement, status request, and category discovery messages.

  Background:
    Given a CAN bus with a server at node 1 and rate limit 500
    And a CAN bus client connected to the same bus

  @REQ-CAN-008
  Scenario: System category is always registered on the server
    When the client sends a category discovery request to node 1
    Then the category list response shall contain category 0

  @REQ-CAN-009
  Scenario: Server sends heartbeat after configured interval
    When 1 second elapses
    Then the server shall have transmitted a heartbeat frame
    And the heartbeat frame shall contain protocol version 1

  @REQ-CAN-010
  Scenario: Heartbeat carries protocol version byte
    When a heartbeat is sent by the server after 1 second
    Then the heartbeat payload byte 0 shall equal 1

  @REQ-CAN-011
  Scenario: Server acknowledges unknown command with error
    When the client sends a command with category 0 and unknown message type 255 to the server
    Then the server shall send an acknowledgement with status "unknownCommand"

  @REQ-CAN-012
  Scenario: Status request causes server to send heartbeat
    When the client sends a status request to the server
    Then the server shall respond with a heartbeat message
