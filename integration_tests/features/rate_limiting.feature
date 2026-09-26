Feature: Rate Limiting
  The server enforces a configurable maximum number of received
  messages per second. Excess messages are silently discarded.

  @REQ-CAN-022
  Scenario: Messages within the rate limit are accepted
    Given a CAN bus with a server at node 1 and rate limit 3
    And a CAN bus client connected to the same bus
    When the client sends 3 heartbeat messages to the server
    Then the server shall have processed 3 messages

  Scenario: Messages exceeding the rate limit are silently discarded
    Given a CAN bus with a server at node 1 and rate limit 3
    And a CAN bus client connected to the same bus
    When the client sends 5 heartbeat messages to the server
    Then the server shall have processed 3 messages

  Scenario: Rate counter resets after one second
    Given a CAN bus with a server at node 1 and rate limit 2
    And a CAN bus client connected to the same bus
    When the client sends 2 heartbeat messages to the server
    And 1 second elapses
    And the client sends 1 heartbeat messages to the server
    Then the server shall have processed 3 messages

  @REQ-CAN-022
  Scenario: An emergency command is accepted after the ordinary rate limit is exhausted
    Given a CAN bus with a server at node 1 and rate limit 3
    And a CAN bus client connected to the same bus
    And a sequenced test category with ID 3 is registered on the server
    When the client sends 3 heartbeat messages to the server
    And the client sends a command to category 3 with sequence number 1
    And the client sends an emergency command to category 3 with sequence number 1
    Then the server category handler shall have received 1 command
