Feature: Custom Category Integration
  Applications extend the protocol by registering their own category
  (IDs 0x2-0xF) on both client and server. The demo category exercises
  the shapes any consumer-defined category needs: a fire-and-forget
  command, a command carrying a payload, a query/response round trip,
  and a category-specific error.

  Background:
    Given a CAN bus with a server at node 1 and rate limit 500
    And a CAN bus client connected to the same bus
    And the demo category is registered on both client and server

  Scenario: Client sends a command without payload
    When the client sends a ping command
    Then the server observer shall receive an OnPing event
    When the server completes the ping command
    Then the server shall send an acknowledgement with status "success"

  Scenario: Client sends a command carrying a payload
    When the client sends a set parameters command with values 100, 200, 50
    Then the server observer shall receive parameters 100, 200, 50

  Scenario: Client queries a value and receives the response
    When the client sends a query value command
    Then the server observer shall receive an OnQueryValue event
    When the server responds with value 1234
    Then the client observer shall receive a value response of 1234

  Scenario: Server reports a category-specific error
    When the client sends a failing command
    Then the server observer shall receive an OnFail event
    When the server reports category error "busy"
    Then the client observer shall receive a category error "busy" for command 3
    And the server shall send an acknowledgement with status "categoryError"

  Scenario: Custom category appears in category discovery
    When the client sends a category discovery request to node 1
    Then the category list response shall contain category 3
