Feature: Category Discovery
  The client can discover which categories a server supports by
  sending a category list request. The server responds with the
  IDs of all registered category handlers.

  Background:
    Given a CAN bus with a server at node 1 and rate limit 500
    And a CAN bus client connected to the same bus

  # REQ-CAN-025: Category discovery
  Scenario: Client discovers only the system category on a fresh server
    When the client sends a category discovery request to node 1
    Then the category list response shall contain 1 category
    And the category list response shall contain category 0

  Scenario: Client discovers system and custom categories
    Given a custom category with ID 5 is registered on the server
    When the client sends a category discovery request to node 1
    Then the category list response shall contain 2 categories
    And the category list response shall contain category 0
    And the category list response shall contain category 5

  # REQ-CAN-014: Application-defined categories
  Scenario: Multiple custom categories are discoverable
    Given a custom category with ID 1 is registered on the server
    And a custom category with ID 5 is registered on the server
    When the client sends a category discovery request to node 1
    Then the category list response shall contain 3 categories
    And the category list response shall contain category 0
    And the category list response shall contain category 1
    And the category list response shall contain category 5

  # Bad flow: discovery without pending callback
  Scenario: Category list response without pending request is ignored
    When the server sends a category list response without a pending request
    Then the client shall silently ignore the response
