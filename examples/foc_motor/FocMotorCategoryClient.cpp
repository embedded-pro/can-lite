#include "examples/foc_motor/FocMotorCategoryClient.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace services
{
    FocMotorCategoryClient::FocMotorCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource)
        : CanCategoryClient(transport, sequenceSource)
    {
        AddMessageTypes(motorTypeResponse, electricalParamsResponse, mechanicalParamsResponse,
            telemetryElectricalResponse, telemetryStatusResponse, selectControlModeResponse);
    }

    uint8_t FocMotorCategoryClient::Id() const
    {
        return focMotorCategoryId;
    }

    bool FocMotorCategoryClient::SendQueryMotorType(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focQueryMotorTypeId);
    }

    bool FocMotorCategoryClient::SendStart(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focStartId);
    }

    bool FocMotorCategoryClient::SendStop(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focStopId);
    }

    bool FocMotorCategoryClient::SendSetPidCurrent(uint16_t targetNodeId, const FocPidGains& gains)
    {
        CanPayloadWriter payload;
        payload.WriteInt16(gains.kp).WriteInt16(gains.ki).WriteInt16(gains.kd);
        return SendCommand(targetNodeId, focSetPidCurrentId, payload);
    }

    bool FocMotorCategoryClient::SendSetPidSpeed(uint16_t targetNodeId, const FocPidGains& gains)
    {
        CanPayloadWriter payload;
        payload.WriteInt16(gains.kp).WriteInt16(gains.ki).WriteInt16(gains.kd);
        return SendCommand(targetNodeId, focSetPidSpeedId, payload);
    }

    bool FocMotorCategoryClient::SendSetPidPosition(uint16_t targetNodeId, const FocPidGains& gains)
    {
        CanPayloadWriter payload;
        payload.WriteInt16(gains.kp).WriteInt16(gains.ki).WriteInt16(gains.kd);
        return SendCommand(targetNodeId, focSetPidPositionId, payload);
    }

    bool FocMotorCategoryClient::SendIdentifyElectrical(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focIdentifyElectricalId);
    }

    bool FocMotorCategoryClient::SendIdentifyMechanical(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focIdentifyMechanicalId);
    }

    bool FocMotorCategoryClient::SendRequestTelemetry(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focRequestTelemetryId);
    }

    bool FocMotorCategoryClient::SendSetEncoderResolution(uint16_t targetNodeId, uint16_t resolution)
    {
        CanPayloadWriter payload;
        payload.WriteUInt16(resolution);
        return SendCommand(targetNodeId, focSetEncoderResolutionId, payload);
    }

    bool FocMotorCategoryClient::SendSelectControlMode(uint16_t targetNodeId, FocMotorMode mode)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(mode));
        return SendCommand(targetNodeId, focSelectControlModeId, payload);
    }

    bool FocMotorCategoryClient::SendSetTorqueSetpoint(uint16_t targetNodeId, int16_t value)
    {
        CanPayloadWriter payload;
        payload.WriteInt16(value);
        return SendCommand(targetNodeId, focSetTorqueSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetSpeedSetpoint(uint16_t targetNodeId, int16_t value)
    {
        CanPayloadWriter payload;
        payload.WriteInt16(value);
        return SendCommand(targetNodeId, focSetSpeedSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetPositionSetpoint(uint16_t targetNodeId, int16_t value)
    {
        CanPayloadWriter payload;
        payload.WriteInt16(value);
        return SendCommand(targetNodeId, focSetPositionSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendClearFault(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focClearFaultId);
    }

    bool FocMotorCategoryClient::SendEmergencyStop(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focEmergencyStopId, CanPriority::emergency);
    }

    bool FocMotorCategoryClient::SendConfigureTelemetryRate(uint16_t targetNodeId, uint8_t rateHz)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(rateHz);
        return SendCommand(targetNodeId, focConfigureTelemetryRateId, payload);
    }

    void FocMotorCategoryClient::HandleMotorTypeResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto mode = static_cast<FocMotorMode>(reader.ReadUInt8());
        if (!reader.Valid())
            return;
        NotifyObservers([mode](auto& observer)
            {
                observer.OnMotorTypeResponse(mode);
            });
    }

    void FocMotorCategoryClient::HandleElectricalParamsResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        FocElectricalParams params{ reader.ReadInt16(), reader.ReadInt16() };
        if (!reader.Valid())
            return;
        NotifyObservers([&params](auto& observer)
            {
                observer.OnElectricalParamsResponse(params);
            });
    }

    void FocMotorCategoryClient::HandleMechanicalParamsResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        FocMechanicalParams params{ reader.ReadInt16(), reader.ReadInt16() };
        if (!reader.Valid())
            return;
        NotifyObservers([&params](auto& observer)
            {
                observer.OnMechanicalParamsResponse(params);
            });
    }

    void FocMotorCategoryClient::HandleTelemetryElectricalResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        FocTelemetryElectrical telemetry{ reader.ReadInt16(), reader.ReadInt16(), reader.ReadInt16(), reader.ReadInt16() };
        if (!reader.Valid())
            return;
        NotifyObservers([&telemetry](auto& observer)
            {
                observer.OnTelemetryElectricalResponse(telemetry);
            });
    }

    void FocMotorCategoryClient::HandleTelemetryStatusResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto state = static_cast<FocMotorState>(reader.ReadUInt8());
        auto fault = static_cast<FocFaultCode>(reader.ReadUInt8());
        auto speed = reader.ReadInt16();
        auto position = reader.ReadInt16();
        if (!reader.Valid())
            return;
        FocTelemetryStatus status{ state, fault, speed, position };
        NotifyObservers([&status](auto& observer)
            {
                observer.OnTelemetryStatusResponse(status);
            });
    }

    void FocMotorCategoryClient::HandleSelectControlModeResponse(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        auto activeMode = static_cast<FocMotorMode>(reader.ReadUInt8());
        if (!reader.Valid())
            return;
        NotifyObservers([activeMode](auto& observer)
            {
                observer.OnSelectControlModeResponse(activeMode);
            });
    }
}
