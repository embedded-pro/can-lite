#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanFrameCodec.hpp"

namespace services
{
    FocMotorCategoryServer::FocMotorCategoryServer(CanFrameTransport& transport)
        : transport(transport)
        , queryMotorType(*this)
        , start(*this)
        , stop(*this)
        , setPidCurrent(*this)
        , setPidSpeed(*this)
        , setPidPosition(*this)
        , identifyElectrical(*this)
        , identifyMechanical(*this)
        , requestTelemetry(*this)
        , setEncoderResolution(*this)
        , selectControlMode(*this)
        , setTorqueSetpoint(*this)
        , setSpeedSetpoint(*this)
        , setPositionSetpoint(*this)
        , clearFault(*this)
        , emergencyStop(*this)
        , configureTelemetryRate(*this)
    {
        AddMessageType(queryMotorType);
        AddMessageType(start);
        AddMessageType(stop);
        AddMessageType(setPidCurrent);
        AddMessageType(setPidSpeed);
        AddMessageType(setPidPosition);
        AddMessageType(identifyElectrical);
        AddMessageType(identifyMechanical);
        AddMessageType(requestTelemetry);
        AddMessageType(setEncoderResolution);
        AddMessageType(selectControlMode);
        AddMessageType(setTorqueSetpoint);
        AddMessageType(setSpeedSetpoint);
        AddMessageType(setPositionSetpoint);
        AddMessageType(clearFault);
        AddMessageType(emergencyStop);
        AddMessageType(configureTelemetryRate);
    }

    uint8_t FocMotorCategoryServer::Id() const
    {
        return focMotorCategoryId;
    }

    void FocMotorCategoryServer::SendMotorTypeResponse(FocMotorMode mode)
    {
        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(mode));
        transport.SendFrame(CanPriority::response, focMotorCategoryId, focMotorTypeResponseId, data, [] {});
    }

    void FocMotorCategoryServer::SendElectricalParamsResponse(const FocElectricalParams& params)
    {
        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt16(data, 0, params.resistance);
        CanFrameCodec::WriteInt16(data, 2, params.inductance);
        transport.SendFrame(CanPriority::response, focMotorCategoryId, focElectricalParamsResponseId, data, [] {});
    }

    void FocMotorCategoryServer::SendMechanicalParamsResponse(const FocMechanicalParams& params)
    {
        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt16(data, 0, params.inertia);
        CanFrameCodec::WriteInt16(data, 2, params.friction);
        transport.SendFrame(CanPriority::response, focMotorCategoryId, focMechanicalParamsResponseId, data, [] {});
    }

    void FocMotorCategoryServer::SendTelemetryElectricalResponse(const FocTelemetryElectrical& telemetry)
    {
        hal::Can::Message data;
        data.resize(8, 0);
        CanFrameCodec::WriteInt16(data, 0, telemetry.voltage);
        CanFrameCodec::WriteInt16(data, 2, telemetry.maxCurrent);
        CanFrameCodec::WriteInt16(data, 4, telemetry.iq);
        CanFrameCodec::WriteInt16(data, 6, telemetry.id);
        transport.SendFrame(CanPriority::telemetry, focMotorCategoryId, focTelemetryElectricalResponseId, data, [] {});
    }

    void FocMotorCategoryServer::SendTelemetryStatusResponse(const FocTelemetryStatus& status)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        data[0] = static_cast<uint8_t>(status.state);
        data[1] = static_cast<uint8_t>(status.fault);
        CanFrameCodec::WriteInt16(data, 2, status.speed);
        CanFrameCodec::WriteInt16(data, 4, status.position);
        transport.SendFrame(CanPriority::telemetry, focMotorCategoryId, focTelemetryStatusResponseId, data, [] {});
    }

    void FocMotorCategoryServer::SendSelectControlModeResponse(FocMotorMode activeMode)
    {
        hal::Can::Message data;
        data.resize(1, 0);
        data[0] = static_cast<uint8_t>(activeMode);
        transport.SendFrame(CanPriority::response, focMotorCategoryId, focSelectControlModeResponseId, data, [] {});
    }

    void FocMotorCategoryServer::SendCategoryError(uint8_t origCommandId, FocMotorCategoryError errorCode)
    {
        hal::Can::Message data;
        data.push_back(origCommandId);
        data.push_back(static_cast<uint8_t>(errorCode));
        transport.SendFrame(CanPriority::response, focMotorCategoryId, focCategoryErrorResponseId, data, [] {});
        SendCommandAck(origCommandId, CanAckStatus::categoryError);
    }

    // QueryMotorType

    FocMotorCategoryServer::QueryMotorTypeMessageType::QueryMotorTypeMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::QueryMotorTypeMessageType::Id() const
    {
        return focQueryMotorTypeId;
    }

    void FocMotorCategoryServer::QueryMotorTypeMessageType::Handle(const hal::Can::Message& data)
    {
        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnQueryMotorType([&server](FocMotorMode mode)
                    {
                        server.SendMotorTypeResponse(mode);
                        server.SendCommandAck(focQueryMotorTypeId, CanAckStatus::success);
                    });
            });
    }

    // Start

    FocMotorCategoryServer::StartMessageType::StartMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::StartMessageType::Id() const
    {
        return focStartId;
    }

    void FocMotorCategoryServer::StartMessageType::Handle(const hal::Can::Message& data)
    {
        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnStart([&server]()
                    {
                        server.SendCommandAck(focStartId, CanAckStatus::success);
                    });
            });
    }

    // Stop

    FocMotorCategoryServer::StopMessageType::StopMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::StopMessageType::Id() const
    {
        return focStopId;
    }

    void FocMotorCategoryServer::StopMessageType::Handle(const hal::Can::Message& data)
    {
        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnStop([&server]()
                    {
                        server.SendCommandAck(focStopId, CanAckStatus::success);
                    });
            });
    }

    // SetPidCurrent

    FocMotorCategoryServer::SetPidCurrentMessageType::SetPidCurrentMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::SetPidCurrentMessageType::Id() const
    {
        return focSetPidCurrentId;
    }

    void FocMotorCategoryServer::SetPidCurrentMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 7)
        {
            parent.SendCommandAck(focSetPidCurrentId, CanAckStatus::invalidPayload);
            return;
        }

        FocPidGains gains{
            CanFrameCodec::ReadInt16(data, 1),
            CanFrameCodec::ReadInt16(data, 3),
            CanFrameCodec::ReadInt16(data, 5)
        };

        auto& server = parent;
        parent.NotifyObservers([&server, &gains](auto& observer)
            {
                observer.OnSetPidCurrent(gains, [&server]()
                    {
                        server.SendCommandAck(focSetPidCurrentId, CanAckStatus::success);
                    });
            });
    }

    // SetPidSpeed

    FocMotorCategoryServer::SetPidSpeedMessageType::SetPidSpeedMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::SetPidSpeedMessageType::Id() const
    {
        return focSetPidSpeedId;
    }

    void FocMotorCategoryServer::SetPidSpeedMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 7)
        {
            parent.SendCommandAck(focSetPidSpeedId, CanAckStatus::invalidPayload);
            return;
        }

        FocPidGains gains{
            CanFrameCodec::ReadInt16(data, 1),
            CanFrameCodec::ReadInt16(data, 3),
            CanFrameCodec::ReadInt16(data, 5)
        };

        auto& server = parent;
        parent.NotifyObservers([&server, &gains](auto& observer)
            {
                observer.OnSetPidSpeed(gains, [&server]()
                    {
                        server.SendCommandAck(focSetPidSpeedId, CanAckStatus::success);
                    });
            });
    }

    // SetPidPosition

    FocMotorCategoryServer::SetPidPositionMessageType::SetPidPositionMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::SetPidPositionMessageType::Id() const
    {
        return focSetPidPositionId;
    }

    void FocMotorCategoryServer::SetPidPositionMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 7)
        {
            parent.SendCommandAck(focSetPidPositionId, CanAckStatus::invalidPayload);
            return;
        }

        FocPidGains gains{
            CanFrameCodec::ReadInt16(data, 1),
            CanFrameCodec::ReadInt16(data, 3),
            CanFrameCodec::ReadInt16(data, 5)
        };

        auto& server = parent;
        parent.NotifyObservers([&server, &gains](auto& observer)
            {
                observer.OnSetPidPosition(gains, [&server]()
                    {
                        server.SendCommandAck(focSetPidPositionId, CanAckStatus::success);
                    });
            });
    }

    // IdentifyElectrical

    FocMotorCategoryServer::IdentifyElectricalMessageType::IdentifyElectricalMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::IdentifyElectricalMessageType::Id() const
    {
        return focIdentifyElectricalId;
    }

    void FocMotorCategoryServer::IdentifyElectricalMessageType::Handle(const hal::Can::Message& data)
    {
        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnIdentifyElectrical([&server](FocElectricalParams params)
                    {
                        server.SendElectricalParamsResponse(params);
                        server.SendCommandAck(focIdentifyElectricalId, CanAckStatus::success);
                    });
            });
    }

    // IdentifyMechanical

    FocMotorCategoryServer::IdentifyMechanicalMessageType::IdentifyMechanicalMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::IdentifyMechanicalMessageType::Id() const
    {
        return focIdentifyMechanicalId;
    }

    void FocMotorCategoryServer::IdentifyMechanicalMessageType::Handle(const hal::Can::Message& data)
    {
        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnIdentifyMechanical([&server](FocMechanicalParams params)
                    {
                        server.SendMechanicalParamsResponse(params);
                        server.SendCommandAck(focIdentifyMechanicalId, CanAckStatus::success);
                    });
            });
    }

    // RequestTelemetry

    FocMotorCategoryServer::RequestTelemetryMessageType::RequestTelemetryMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::RequestTelemetryMessageType::Id() const
    {
        return focRequestTelemetryId;
    }

    void FocMotorCategoryServer::RequestTelemetryMessageType::Handle(const hal::Can::Message& data)
    {
        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnRequestTelemetry([&server](FocTelemetryElectrical electrical, FocTelemetryStatus status)
                    {
                        server.SendTelemetryElectricalResponse(electrical);
                        server.SendTelemetryStatusResponse(status);
                        server.SendCommandAck(focRequestTelemetryId, CanAckStatus::success);
                    });
            });
    }

    // SetEncoderResolution

    FocMotorCategoryServer::SetEncoderResolutionMessageType::SetEncoderResolutionMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::SetEncoderResolutionMessageType::Id() const
    {
        return focSetEncoderResolutionId;
    }

    void FocMotorCategoryServer::SetEncoderResolutionMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 3)
        {
            parent.SendCommandAck(focSetEncoderResolutionId, CanAckStatus::invalidPayload);
            return;
        }

        auto resolution = static_cast<uint16_t>(CanFrameCodec::ReadInt16(data, 1));

        auto& server = parent;
        parent.NotifyObservers([&server, resolution](auto& observer)
            {
                observer.OnSetEncoderResolution(resolution, [&server]()
                    {
                        server.SendCommandAck(focSetEncoderResolutionId, CanAckStatus::success);
                    });
            });
    }

    // SelectControlMode

    FocMotorCategoryServer::SelectControlModeMessageType::SelectControlModeMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::SelectControlModeMessageType::Id() const
    {
        return focSelectControlModeId;
    }

    void FocMotorCategoryServer::SelectControlModeMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 2)
        {
            parent.SendCommandAck(focSelectControlModeId, CanAckStatus::invalidPayload);
            return;
        }

        auto mode = static_cast<FocMotorMode>(data[1]);
        if (mode != FocMotorMode::torque && mode != FocMotorMode::speed && mode != FocMotorMode::position)
        {
            parent.SendCommandAck(focSelectControlModeId, CanAckStatus::invalidPayload);
            return;
        }

        auto& server = parent;
        parent.NotifyObservers([&server, mode](auto& observer)
            {
                observer.OnSelectControlMode(mode, [&server](FocMotorMode activatedMode)
                    {
                        server.SendSelectControlModeResponse(activatedMode);
                        server.SendCommandAck(focSelectControlModeId, CanAckStatus::success);
                    });
            });
    }

    // SetTorqueSetpoint

    FocMotorCategoryServer::SetTorqueSetpointMessageType::SetTorqueSetpointMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::SetTorqueSetpointMessageType::Id() const
    {
        return focSetTorqueSetpointId;
    }

    void FocMotorCategoryServer::SetTorqueSetpointMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 3)
        {
            parent.SendCommandAck(focSetTorqueSetpointId, CanAckStatus::invalidPayload);
            return;
        }

        auto value = CanFrameCodec::ReadInt16(data, 1);

        auto& server = parent;
        parent.NotifyObservers([&server, value](auto& observer)
            {
                observer.OnSetTorqueSetpoint(value, [&server]()
                    {
                        server.SendCommandAck(focSetTorqueSetpointId, CanAckStatus::success);
                    });
            });
    }

    // SetSpeedSetpoint

    FocMotorCategoryServer::SetSpeedSetpointMessageType::SetSpeedSetpointMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::SetSpeedSetpointMessageType::Id() const
    {
        return focSetSpeedSetpointId;
    }

    void FocMotorCategoryServer::SetSpeedSetpointMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 3)
        {
            parent.SendCommandAck(focSetSpeedSetpointId, CanAckStatus::invalidPayload);
            return;
        }

        auto value = CanFrameCodec::ReadInt16(data, 1);

        auto& server = parent;
        parent.NotifyObservers([&server, value](auto& observer)
            {
                observer.OnSetSpeedSetpoint(value, [&server]()
                    {
                        server.SendCommandAck(focSetSpeedSetpointId, CanAckStatus::success);
                    });
            });
    }

    // SetPositionSetpoint

    FocMotorCategoryServer::SetPositionSetpointMessageType::SetPositionSetpointMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::SetPositionSetpointMessageType::Id() const
    {
        return focSetPositionSetpointId;
    }

    void FocMotorCategoryServer::SetPositionSetpointMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 3)
        {
            parent.SendCommandAck(focSetPositionSetpointId, CanAckStatus::invalidPayload);
            return;
        }

        auto value = CanFrameCodec::ReadInt16(data, 1);

        auto& server = parent;
        parent.NotifyObservers([&server, value](auto& observer)
            {
                observer.OnSetPositionSetpoint(value, [&server]()
                    {
                        server.SendCommandAck(focSetPositionSetpointId, CanAckStatus::success);
                    });
            });
    }

    // ClearFault

    FocMotorCategoryServer::ClearFaultMessageType::ClearFaultMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::ClearFaultMessageType::Id() const
    {
        return focClearFaultId;
    }

    void FocMotorCategoryServer::ClearFaultMessageType::Handle(const hal::Can::Message& data)
    {
        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnClearFault([&server]()
                    {
                        server.SendCommandAck(focClearFaultId, CanAckStatus::success);
                    });
            });
    }

    // EmergencyStop

    FocMotorCategoryServer::EmergencyStopMessageType::EmergencyStopMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::EmergencyStopMessageType::Id() const
    {
        return focEmergencyStopId;
    }

    void FocMotorCategoryServer::EmergencyStopMessageType::Handle(const hal::Can::Message& data)
    {
        auto& server = parent;
        parent.NotifyObservers([&server](auto& observer)
            {
                observer.OnEmergencyStop([&server]()
                    {
                        server.SendCommandAck(focEmergencyStopId, CanAckStatus::success);
                    });
            });
    }

    // ConfigureTelemetryRate

    FocMotorCategoryServer::ConfigureTelemetryRateMessageType::ConfigureTelemetryRateMessageType(FocMotorCategoryServer& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryServer::ConfigureTelemetryRateMessageType::Id() const
    {
        return focConfigureTelemetryRateId;
    }

    void FocMotorCategoryServer::ConfigureTelemetryRateMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 2)
        {
            parent.SendCommandAck(focConfigureTelemetryRateId, CanAckStatus::invalidPayload);
            return;
        }

        auto rateHz = data[1];

        auto& server = parent;
        parent.NotifyObservers([&server, rateHz](auto& observer)
            {
                observer.OnConfigureTelemetryRate(rateHz, [&server]()
                    {
                        server.SendCommandAck(focConfigureTelemetryRateId, CanAckStatus::success);
                    });
            });
    }
}
