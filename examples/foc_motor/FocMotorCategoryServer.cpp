#include "examples/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace services
{
    FocMotorCategoryServer::FocMotorCategoryServer(CanFrameTransport& transport)
        : CanCategoryServer(transport)
    {
        AddMessageTypes(queryMotorType, start, stop, setPidCurrent, setPidSpeed, setPidPosition,
            identifyElectrical, identifyMechanical, requestTelemetry, setEncoderResolution,
            selectControlMode, setTorqueSetpoint, setSpeedSetpoint, setPositionSetpoint,
            clearFault, emergencyStop, configureTelemetryRate);
    }

    uint8_t FocMotorCategoryServer::Id() const
    {
        return focMotorCategoryId;
    }

    void FocMotorCategoryServer::SendMotorTypeResponse(FocMotorMode mode)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(mode));
        SendResponse(focMotorTypeResponseId, payload);
    }

    void FocMotorCategoryServer::SendElectricalParamsResponse(const FocElectricalParams& params)
    {
        CanPayloadWriter payload;
        payload.WriteFixed16(params.resistance, focResistanceScale)
            .WriteFixed16(params.inductance, focInductanceScale);
        SendResponse(focElectricalParamsResponseId, payload);
    }

    void FocMotorCategoryServer::SendMechanicalParamsResponse(const FocMechanicalParams& params)
    {
        CanPayloadWriter payload;
        payload.WriteFixed16(params.inertia, focInertiaScale)
            .WriteFixed16(params.friction, focFrictionScale);
        SendResponse(focMechanicalParamsResponseId, payload);
    }

    void FocMotorCategoryServer::SendTelemetryElectricalResponse(const FocTelemetryElectrical& telemetry)
    {
        CanPayloadWriter payload;
        payload.WriteFixed16(telemetry.voltage, focVoltageScale)
            .WriteFixed16(telemetry.maxCurrent, focCurrentScale)
            .WriteFixed16(telemetry.iq, focCurrentScale)
            .WriteFixed16(telemetry.id, focCurrentScale);
        SendTelemetry(focTelemetryElectricalResponseId, payload);
    }

    void FocMotorCategoryServer::SendTelemetryStatusResponse(const FocTelemetryStatus& status)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(status.state))
            .WriteUInt8(static_cast<uint8_t>(status.fault))
            .WriteFixed16(status.speed, focSpeedScale)
            .WriteFixed16(status.position, focPositionScale);
        SendTelemetry(focTelemetryStatusResponseId, payload);
    }

    void FocMotorCategoryServer::SendSelectControlModeResponse(FocMotorMode activeMode)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(activeMode));
        SendResponse(focSelectControlModeResponseId, payload);
    }

    void FocMotorCategoryServer::SendCategoryError(uint8_t origCommandId, FocMotorCategoryError errorCode)
    {
        CanCategoryServer::SendCategoryError(origCommandId, static_cast<uint8_t>(errorCode));
        SendCommandAck(origCommandId, CanAckStatus::categoryError);
    }

    void FocMotorCategoryServer::HandleQueryMotorType(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnQueryMotorType([this](FocMotorMode mode)
                    {
                        SendMotorTypeResponse(mode);
                        SendCommandAck(focQueryMotorTypeId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleStart(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnStart([this](CanAckStatus status)
                    {
                        SendCommandAck(focStartId, status);
                    });
            });
    }

    void FocMotorCategoryServer::HandleStop(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnStop([this]()
                    {
                        SendCommandAck(focStopId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetPidCurrent(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        FocPidGains gains{ reader.ReadInt16(), reader.ReadInt16(), reader.ReadInt16() };
        if (!reader.Valid())
        {
            SendCommandAck(focSetPidCurrentId, CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, gains](auto& observer)
            {
                observer.OnSetPidCurrent(gains, [this]()
                    {
                        SendCommandAck(focSetPidCurrentId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetPidSpeed(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        FocPidGains gains{ reader.ReadInt16(), reader.ReadInt16(), reader.ReadInt16() };
        if (!reader.Valid())
        {
            SendCommandAck(focSetPidSpeedId, CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, gains](auto& observer)
            {
                observer.OnSetPidSpeed(gains, [this]()
                    {
                        SendCommandAck(focSetPidSpeedId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetPidPosition(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        FocPidGains gains{ reader.ReadInt16(), reader.ReadInt16(), reader.ReadInt16() };
        if (!reader.Valid())
        {
            SendCommandAck(focSetPidPositionId, CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, gains](auto& observer)
            {
                observer.OnSetPidPosition(gains, [this]()
                    {
                        SendCommandAck(focSetPidPositionId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleIdentifyElectrical(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnIdentifyElectrical([this](FocElectricalParams params)
                    {
                        SendElectricalParamsResponse(params);
                        SendCommandAck(focIdentifyElectricalId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleIdentifyMechanical(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnIdentifyMechanical([this](FocMechanicalParams params)
                    {
                        SendMechanicalParamsResponse(params);
                        SendCommandAck(focIdentifyMechanicalId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleRequestTelemetry(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnRequestTelemetry([this](FocTelemetryElectrical electrical, FocTelemetryStatus status)
                    {
                        SendTelemetryElectricalResponse(electrical);
                        SendTelemetryStatusResponse(status);
                        SendCommandAck(focRequestTelemetryId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetEncoderResolution(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto resolution = reader.ReadUInt16();
        if (!reader.Valid())
        {
            SendCommandAck(focSetEncoderResolutionId, CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, resolution](auto& observer)
            {
                observer.OnSetEncoderResolution(resolution, [this]()
                    {
                        SendCommandAck(focSetEncoderResolutionId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSelectControlMode(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto mode = static_cast<FocMotorMode>(reader.ReadUInt8());
        if (!reader.Valid())
        {
            SendCommandAck(focSelectControlModeId, CanAckStatus::invalidPayload);
            return;
        }
        if (mode != FocMotorMode::torque && mode != FocMotorMode::speed && mode != FocMotorMode::position)
        {
            SendCommandAck(focSelectControlModeId, CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, mode](auto& observer)
            {
                observer.OnSelectControlMode(mode, [this](FocMotorMode activatedMode)
                    {
                        SendSelectControlModeResponse(activatedMode);
                        SendCommandAck(focSelectControlModeId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetTorqueSetpoint(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto value = reader.ReadFixed16(focCurrentScale);
        if (!reader.Valid())
        {
            SendCommandAck(focSetTorqueSetpointId, CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, value](auto& observer)
            {
                observer.OnSetTorqueSetpoint(value, [this]()
                    {
                        SendCommandAck(focSetTorqueSetpointId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetSpeedSetpoint(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto value = reader.ReadFixed16(focSpeedScale);
        if (!reader.Valid())
        {
            SendCommandAck(focSetSpeedSetpointId, CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, value](auto& observer)
            {
                observer.OnSetSpeedSetpoint(value, [this]()
                    {
                        SendCommandAck(focSetSpeedSetpointId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetPositionSetpoint(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto value = reader.ReadFixed16(focPositionScale);
        if (!reader.Valid())
        {
            SendCommandAck(focSetPositionSetpointId, CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, value](auto& observer)
            {
                observer.OnSetPositionSetpoint(value, [this]()
                    {
                        SendCommandAck(focSetPositionSetpointId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleClearFault(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnClearFault([this]()
                    {
                        SendCommandAck(focClearFaultId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleEmergencyStop(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnEmergencyStop([this]()
                    {
                        SendCommandAck(focEmergencyStopId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleConfigureTelemetryRate(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto rateHz = reader.ReadUInt8();
        if (!reader.Valid())
        {
            SendCommandAck(focConfigureTelemetryRateId, CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, rateHz](auto& observer)
            {
                observer.OnConfigureTelemetryRate(rateHz, [this]()
                    {
                        SendCommandAck(focConfigureTelemetryRateId, CanAckStatus::success);
                    });
            });
    }
}
