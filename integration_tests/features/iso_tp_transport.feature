Feature: ISO-TP Transport
  Multi-frame PDUs travel between the client and the server through a real
  ISO-TP transport on each node. A transfer that fails is discarded and
  reported to the application on each side through the transport's abort
  callback, and the channel stays open for the next transfer.

  Background:
    Given a CAN bus with a server at node 1 and rate limit 500
    And a CAN bus client connected to the same bus
    And ISO-TP transports connect the client and the server

  @REQ-CAN-029
  Scenario: A multi-frame PDU reaches the category's PDU handler
    When the client application sends a 20-byte PDU to the PDU test category
    Then the PDU test category shall have received a 20-byte PDU
    And the client application shall have been told 1 transfer completed
    And neither application shall have been told of a failed transfer

  @REQ-CAN-046
  Scenario: Lost flow control fails the transfer on both nodes and the channel survives
    Given the bus loses the server's flow control frames
    When the client application sends a 20-byte PDU to the PDU test category
    And 1 second elapses
    Then the client application shall have been told the transfer failed with reason nBsTimeout
    And the server application shall have been told the reception failed with reason nCrTimeout
    And the PDU test category shall have received 0 PDUs
    When the bus delivers every frame again
    And the client application sends a 20-byte PDU to the PDU test category
    Then the PDU test category shall have received a 20-byte PDU
    And the client application shall have been told 1 transfer completed

  @REQ-CAN-046
  Scenario: A frame the driver fails to send aborts the transfer
    Given the client's driver fails every frame of the transfer
    When the client application sends a 20-byte PDU to the PDU test category
    Then the client application shall have been told the transfer failed with reason sendFailed
    And the server application shall not have been told of a failed reception
    And the PDU test category shall have received 0 PDUs
