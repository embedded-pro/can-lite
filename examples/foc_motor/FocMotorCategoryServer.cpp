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
        payload.WriteInt16(params.resistance).WriteInt16(params.inductance);
        SendResponse(focElectricalParamsResponseId, payload);
    }

    void FocMotorCategoryServer::SendMechanicalParamsResponse(const FocMechanicalParams& params)
    {
        CanPayloadWriter payload;
        payload.WriteInt16(params.inertia).WriteInt16(params.friction);
        SendResponse(focMechanicalParamsResponseId, payload);
    }

    void FocMotorCategoryServer::SendTelemetryElectricalResponse(const FocTelemetryElectrical& telemetry)
    {
        CanPayloadWriter payload;
        payload.WriteInt16(telemetry.voltage).WriteInt16(telemetry.maxCurrent).WriteInt16(telemetry.iq).WriteInt16(telemetry.id);
        SendTelemetry(focTelemetryElectricalResponseId, payload);
    }

    void FocMotorCategoryServer::SendTelemetryStatusResponse(const FocTelemetryStatus& status)
    {
        CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(status.state))
            .WriteUInt8(static_cast<uint8_t>(status.fault))
            .WriteInt16(status.speed)
            .WriteInt16(status.position);
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
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnQueryMotorType([&server](FocMotorMode mode)
                    {
                        server.SendMotorTypeResponse(mode);
                        server.SendCommandAck(focQueryMotorTypeId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleStart(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnStart([&server]()
                    {
                        server.SendCommandAck(focStartId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleStop(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnStop([&server]()
                    {
                        server.SendCommandAck(focStopId, CanAckStatus::success);
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
        auto& server = *this;
        NotifyObservers([&server, gains](auto& observer)
            {
                observer.OnSetPidCurrent(gains, [&server]()
                    {
                        server.SendCommandAck(focSetPidCurrentId, CanAckStatus::success);
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
        auto& server = *this;
        NotifyObservers([&server, gains](auto& observer)
            {
                observer.OnSetPidSpeed(gains, [&server]()
                    {
                        server.SendCommandAck(focSetPidSpeedId, CanAckStatus::success);
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
        auto& server = *this;
        NotifyObservers([&server, gains](auto& observer)
            {
                observer.OnSetPidPosition(gains, [&server]()
                    {
                        server.SendCommandAck(focSetPidPositionId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleIdentifyElectrical(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnIdentifyElectrical([&server](FocElectricalParams params)
                    {
                        server.SendElectricalParamsResponse(params);
                        server.SendCommandAck(focIdentifyElectricalId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleIdentifyMechanical(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnIdentifyMechanical([&server](FocMechanicalParams params)
                    {
                        server.SendMechanicalParamsResponse(params);
                        server.SendCommandAck(focIdentifyMechanicalId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleRequestTelemetry(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnRequestTelemetry([&server](FocTelemetryElectrical electrical, FocTelemetryStatus status)
                    {
                        server.SendTelemetryElectricalResponse(electrical);
                        server.SendTelemetryStatusResponse(status);
                        server.SendCommandAck(focRequestTelemetryId, CanAckStatus::success);
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
        auto& server = *this;
        NotifyObservers([&server, resolution](auto& observer)
            {
                observer.OnSetEncoderResolution(resolution, [&server]()
                    {
                        server.SendCommandAck(focSetEncoderResolutionId, CanAckStatus::success);
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
        auto& server = *this;
        NotifyObservers([&server, mode](auto& observer)
            {
                observer.OnSelectControlMode(mode, [&server](FocMotorMode activatedMode)
                    {
                        server.SendSelectControlModeResponse(activatedMode);
                        server.SendCommandAck(focSelectControlModeId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetTorqueSetpoint(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto value = reader.ReadInt16();
        if (!reader.Valid())
        {
            SendCommandAck(focSetTorqueSetpointId, CanAckStatus::invalidPayload);
            return;
        }
        auto& server = *this;
        NotifyObservers([&server, value](auto& observer)
            {
                observer.OnSetTorqueSetpoint(value, [&server]()
                    {
                        server.SendCommandAck(focSetTorqueSetpointId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetSpeedSetpoint(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto value = reader.ReadInt16();
        if (!reader.Valid())
        {
            SendCommandAck(focSetSpeedSetpointId, CanAckStatus::invalidPayload);
            return;
        }
        auto& server = *this;
        NotifyObservers([&server, value](auto& observer)
            {
                observer.OnSetSpeedSetpoint(value, [&server]()
                    {
                        server.SendCommandAck(focSetSpeedSetpointId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetPositionSetpoint(const hal::Can::Message& data)
    {
        CanPayloadReader reader{ data };
        reader.Skip(1);
        auto value = reader.ReadInt16();
        if (!reader.Valid())
        {
            SendCommandAck(focSetPositionSetpointId, CanAckStatus::invalidPayload);
            return;
        }
        auto& server = *this;
        NotifyObservers([&server, value](auto& observer)
            {
                observer.OnSetPositionSetpoint(value, [&server]()
                    {
                        server.SendCommandAck(focSetPositionSetpointId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleClearFault(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnClearFault([&server]()
                    {
                        server.SendCommandAck(focClearFaultId, CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleEmergencyStop(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnEmergencyStop([&server]()
                    {
                        server.SendCommandAck(focEmergencyStopId, CanAckStatus::success);
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
        auto& server = *this;
        NotifyObservers([&server, rateHz](auto& observer)
            {
                observer.OnConfigureTelemetryRate(rateHz, [&server]()
                    {
                        server.SendCommandAck(focConfigureTelemetryRateId, CanAckStatus::success);
                    });
            });
    }
}
