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
    And the client shall have received a begin upgrade response with status ok

  @REQ-FWU-016
  Scenario: A server without a firmware upgrade application answers not ready
    Given the server application has detached from the firmware upgrade category
    When the client sends a begin upgrade command
    Then the client shall have received a begin upgrade response with status not ready

  @REQ-FWU-015
  Scenario: Session timeout fires after inactivity
    When the client sends a begin upgrade command with firmware size 1024
    And 30 seconds elapse
    Then the firmware upgrade session shall have timed out

  @REQ-FWU-015
  Scenario: A refused begin upgrade opens no session
    When the client sends a begin upgrade command that the server refuses as busy
    And 60 seconds elapse
    Then the client shall have received a begin upgrade response with status busy
    And the firmware upgrade session shall not have timed out

  @REQ-FWU-015
  Scenario: Data block resets the session timeout
    When the client sends a begin upgrade command with firmware size 12
    And 25 seconds elapse
    And the client sends data block 0
    And 25 seconds elapse
    Then the firmware upgrade session shall not have timed out

  @REQ-FWU-015
  Scenario: Abort stops the session timeout
    When the client sends a begin upgrade command with firmware size 12
    And the client sends an abort command
    And 60 seconds elapse
    Then the firmware upgrade session shall not have timed out

  @REQ-FWU-015
  Scenario: Verify stops the session timeout
    When the client sends a begin upgrade command with firmware size 12
    And the client sends a verify command with CRC32 305419896
    And 60 seconds elapse
    Then the firmware upgrade session shall not have timed out

  @REQ-FWU-009
  Scenario: Query progress is forwarded to observer
    When the client sends a query progress command
    Then the firmware upgrade server observer shall have received a query progress notification
