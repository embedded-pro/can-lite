Feature: FOC Motor Category Integration
  The FOC Motor Control category (0x02) implements field-oriented
  motor control commands and telemetry. This validates the
  end-to-end integration between client and server for this category.

  Background:
    Given a CAN bus with a server at node 1 and rate limit 500
    And a CAN bus client connected to the same bus
    And the FOC motor category is registered on both client and server

  # Client sends command -> Server receives -> Server sends response -> Client receives
  Scenario: Client queries motor type and receives response
    When the client sends a query motor type command
    Then the server observer shall receive an OnQueryMotorType event
    When the server sends a motor type response with mode "torque"
    Then the client observer shall receive a motor type response with mode "torque"

  Scenario: Client starts and stops motor
    When the client sends a start command
    Then the server observer shall receive an OnStart event
    When the client sends a stop command
    Then the server observer shall receive an OnStop event

  Scenario: Client sets PID current gains
    When the client sends a set PID current command with kp 100, ki 200, kd 50
    Then the server observer shall receive PID current gains kp 100, ki 200, kd 50

  Scenario: Client sends set PID speed gains
    When the client sends a set PID speed command with kp 300, ki 150, kd 75
    Then the server observer shall receive PID speed gains kp 300, ki 150, kd 75

  Scenario: Client sends set PID position gains
    When the client sends a set PID position command with kp 500, ki 250, kd 125
    Then the server observer shall receive PID position gains kp 500, ki 250, kd 125

  Scenario: Client requests electrical identification
    When the client sends an identify electrical command
    Then the server observer shall receive an OnIdentifyElectrical event

  Scenario: Client requests mechanical identification
    When the client sends an identify mechanical command
    Then the server observer shall receive an OnIdentifyMechanical event

  Scenario: Server sends electrical params response to client
    When the server sends electrical params with resistance 1500 and inductance 300
    Then the client observer shall receive electrical params with resistance 1500 and inductance 300

  Scenario: Server sends mechanical params response to client
    When the server sends mechanical params with inertia 4000 and friction 2000
    Then the client observer shall receive mechanical params with inertia 4000 and friction 2000

  Scenario: Client requests telemetry and receives electrical telemetry
    When the client sends a request telemetry command
    Then the server observer shall receive an OnRequestTelemetry event
    When the server sends electrical telemetry with voltage 120, maxCurrent 50, iq 30, id 10
    Then the client observer shall receive electrical telemetry with voltage 120, maxCurrent 50, iq 30, id 10

  Scenario: Server sends telemetry status response to client
    When the server sends telemetry status with state "running", fault "none", speed 1500, position 3600
    Then the client observer shall receive telemetry status with state "running", fault "none", speed 1500, position 3600

  Scenario: Client sets encoder resolution
    When the client sends a set encoder resolution command with resolution 4096
    Then the server observer shall receive encoder resolution 4096

  Scenario: FOC motor category appears in category discovery
    When the client sends a category discovery request to node 1
    Then the category list response shall contain category 2
