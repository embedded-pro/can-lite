Feature: Firmware Upgrade
  The firmware upgrade category provides block-based firmware transfer
  with session timeout protection.

  Background:
    Given a CAN bus with a server at node 1 and rate limit 500
    And a CAN bus client connected to the same bus
    And the firmware upgrade category is registered on both client and server

  @REQ-FWU-001
  Scenario: Successful begin upgrade is forwarded to observer
    When the client sends a begin upgrade command with firmware size 12288
    Then the firmware upgrade server observer shall have received a begin upgrade with size 12288

  @REQ-FWU-015
  Scenario: Session timeout fires after inactivity
    When the client sends a begin upgrade command with firmware size 1024
    And 30 seconds elapse expecting a session timeout

  @REQ-FWU-015
  Scenario: Data block resets the session timeout
    When the client sends a begin upgrade command with firmware size 12
    And 25 seconds elapse without a session timeout
    And the client sends data block 0
    And 25 seconds elapse without a session timeout

  @REQ-FWU-015
  Scenario: Abort stops the session timeout
    When the client sends a begin upgrade command with firmware size 12
    And the client sends an abort command
    And 60 seconds elapse without a session timeout

  @REQ-FWU-015
  Scenario: Verify stops the session timeout
    When the client sends a begin upgrade command with firmware size 12
    And the client sends a verify command with CRC32 305419896
    And 60 seconds elapse without a session timeout

  @REQ-FWU-009
  Scenario: Query progress is forwarded to observer
    When the client sends a query progress command
    Then the firmware upgrade server observer shall have received a query progress notification
