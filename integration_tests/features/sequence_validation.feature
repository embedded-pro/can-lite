Feature: Sequence Validation
  The server validates command frames using an 8-bit sequence counter.
  The first command is accepted regardless of its sequence value.
  Subsequent commands must have sequence = (previous + 1) mod 256.

  Background:
    Given a CAN bus with a server at node 1 and rate limit 500
    And a CAN bus client connected to the same bus
    And a sequenced test category with ID 3 is registered on the server

  # REQ-CAN-017: Sequence number validation
  Scenario: First command is accepted regardless of sequence value
    When the client sends a command to category 3 with sequence number 42
    Then the server category handler shall have received 1 command

  Scenario: Sequential commands are accepted
    When the client sends a command to category 3 with sequence number 1
    And the client sends a command to category 3 with sequence number 2
    And the client sends a command to category 3 with sequence number 3
    Then the server category handler shall have received 3 commands

  Scenario: Duplicate sequence number is rejected
    When the client sends a command to category 3 with sequence number 1
    And the client sends a command to category 3 with sequence number 1
    Then the server category handler shall have received 1 command
    And the server shall send an acknowledgement with status "sequenceError"

  Scenario: Out-of-order sequence number is rejected
    When the client sends a command to category 3 with sequence number 1
    And the client sends a command to category 3 with sequence number 5
    Then the server category handler shall have received 1 command
    And the server shall send an acknowledgement with status "sequenceError"

  # REQ-CAN-018: Sequence number wrap-around
  Scenario: Sequence counter wraps from 255 to 0
    When the client sends a command to category 3 with sequence number 255
    And the client sends a command to category 3 with sequence number 0
    Then the server category handler shall have received 2 commands

  # REQ-CAN-019: Sequence bypass per category
  Scenario: System category does not require sequence validation
    When the client sends a status request to the server with empty payload
    Then the server shall respond with a heartbeat message

  # Bad flow: empty payload on sequenced category
  Scenario: Empty payload on sequenced category is rejected with invalidPayload
    When the client sends a command to category 3 with empty payload
    Then the server category handler shall have received 0 commands
    And the server shall send an acknowledgement with status "invalidPayload"
