Feature: Rate Limiting
  The server enforces a configurable maximum number of received
  messages per second. Excess messages are silently discarded.

  # REQ-CAN-022: Message rate limiting
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
