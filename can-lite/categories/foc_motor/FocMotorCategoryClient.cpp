#include "can-lite/categories/foc_motor/FocMotorCategoryClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"

namespace services
{
    FocMotorCategoryClient::FocMotorCategoryClient(CanFrameTransport& transport)
        : transport(transport)
        , motorTypeResponse(*this)
        , electricalParamsResponse(*this)
        , mechanicalParamsResponse(*this)
        , telemetryElectricalResponse(*this)
        , telemetryStatusResponse(*this)
    {
        AddMessageType(motorTypeResponse);
        AddMessageType(electricalParamsResponse);
        AddMessageType(mechanicalParamsResponse);
        AddMessageType(telemetryElectricalResponse);
        AddMessageType(telemetryStatusResponse);
    }

    uint8_t FocMotorCategoryClient::Id() const
    {
        return focMotorCategoryId;
    }

    void FocMotorCategoryClient::SendSimpleCommand(uint8_t messageTypeId)
    {
        hal::Can::Message data;
        data.push_back(sequenceCounter++);
        transport.SendFrame(CanPriority::command, focMotorCategoryId, messageTypeId, data, [] {});
    }

    void FocMotorCategoryClient::SendQueryMotorType()
    {
        SendSimpleCommand(focQueryMotorTypeId);
    }

    void FocMotorCategoryClient::SendStart()
    {
        SendSimpleCommand(focStartId);
    }

    void FocMotorCategoryClient::SendStop()
    {
        SendSimpleCommand(focStopId);
    }

    void FocMotorCategoryClient::SendSetPidCurrent(const FocPidGains& gains)
    {
        hal::Can::Message data;
        data.resize(7, 0);
        data[0] = sequenceCounter++;
        CanFrameCodec::WriteInt16(data, 1, gains.kp);
        CanFrameCodec::WriteInt16(data, 3, gains.ki);
        CanFrameCodec::WriteInt16(data, 5, gains.kd);
        transport.SendFrame(CanPriority::command, focMotorCategoryId, focSetPidCurrentId, data, [] {});
    }

    void FocMotorCategoryClient::SendSetPidSpeed(const FocPidGains& gains)
    {
        hal::Can::Message data;
        data.resize(7, 0);
        data[0] = sequenceCounter++;
        CanFrameCodec::WriteInt16(data, 1, gains.kp);
        CanFrameCodec::WriteInt16(data, 3, gains.ki);
        CanFrameCodec::WriteInt16(data, 5, gains.kd);
        transport.SendFrame(CanPriority::command, focMotorCategoryId, focSetPidSpeedId, data, [] {});
    }

    void FocMotorCategoryClient::SendSetPidPosition(const FocPidGains& gains)
    {
        hal::Can::Message data;
        data.resize(7, 0);
        data[0] = sequenceCounter++;
        CanFrameCodec::WriteInt16(data, 1, gains.kp);
        CanFrameCodec::WriteInt16(data, 3, gains.ki);
        CanFrameCodec::WriteInt16(data, 5, gains.kd);
        transport.SendFrame(CanPriority::command, focMotorCategoryId, focSetPidPositionId, data, [] {});
    }

    void FocMotorCategoryClient::SendIdentifyElectrical()
    {
        SendSimpleCommand(focIdentifyElectricalId);
    }

    void FocMotorCategoryClient::SendIdentifyMechanical()
    {
        SendSimpleCommand(focIdentifyMechanicalId);
    }

    void FocMotorCategoryClient::SendRequestTelemetry()
    {
        SendSimpleCommand(focRequestTelemetryId);
    }

    void FocMotorCategoryClient::SendSetEncoderResolution(uint16_t resolution)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = sequenceCounter++;
        CanFrameCodec::WriteInt16(data, 1, static_cast<int16_t>(resolution));
        transport.SendFrame(CanPriority::command, focMotorCategoryId, focSetEncoderResolutionId, data, [] {});
    }

    // MotorTypeResponse

    FocMotorCategoryClient::MotorTypeResponseMessageType::MotorTypeResponseMessageType(FocMotorCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryClient::MotorTypeResponseMessageType::Id() const
    {
        return focMotorTypeResponseId;
    }

    void FocMotorCategoryClient::MotorTypeResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.empty())
            return;

        auto mode = static_cast<FocMotorMode>(data[0]);

        parent.NotifyObservers([mode](auto& observer)
            {
                observer.OnMotorTypeResponse(mode);
            });
    }

    // ElectricalParamsResponse

    FocMotorCategoryClient::ElectricalParamsResponseMessageType::ElectricalParamsResponseMessageType(FocMotorCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryClient::ElectricalParamsResponseMessageType::Id() const
    {
        return focElectricalParamsResponseId;
    }

    void FocMotorCategoryClient::ElectricalParamsResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 4)
            return;

        FocElectricalParams params{
            CanFrameCodec::ReadInt16(data, 0),
            CanFrameCodec::ReadInt16(data, 2)
        };

        parent.NotifyObservers([&params](auto& observer)
            {
                observer.OnElectricalParamsResponse(params);
            });
    }

    // MechanicalParamsResponse

    FocMotorCategoryClient::MechanicalParamsResponseMessageType::MechanicalParamsResponseMessageType(FocMotorCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryClient::MechanicalParamsResponseMessageType::Id() const
    {
        return focMechanicalParamsResponseId;
    }

    void FocMotorCategoryClient::MechanicalParamsResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 4)
            return;

        FocMechanicalParams params{
            CanFrameCodec::ReadInt16(data, 0),
            CanFrameCodec::ReadInt16(data, 2)
        };

        parent.NotifyObservers([&params](auto& observer)
            {
                observer.OnMechanicalParamsResponse(params);
            });
    }

    // TelemetryElectricalResponse

    FocMotorCategoryClient::TelemetryElectricalResponseMessageType::TelemetryElectricalResponseMessageType(FocMotorCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryClient::TelemetryElectricalResponseMessageType::Id() const
    {
        return focTelemetryElectricalResponseId;
    }

    void FocMotorCategoryClient::TelemetryElectricalResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 8)
            return;

        FocTelemetryElectrical telemetry{
            CanFrameCodec::ReadInt16(data, 0),
            CanFrameCodec::ReadInt16(data, 2),
            CanFrameCodec::ReadInt16(data, 4),
            CanFrameCodec::ReadInt16(data, 6)
        };

        parent.NotifyObservers([&telemetry](auto& observer)
            {
                observer.OnTelemetryElectricalResponse(telemetry);
            });
    }

    // TelemetryStatusResponse

    FocMotorCategoryClient::TelemetryStatusResponseMessageType::TelemetryStatusResponseMessageType(FocMotorCategoryClient& parent)
        : parent(parent)
    {}

    uint8_t FocMotorCategoryClient::TelemetryStatusResponseMessageType::Id() const
    {
        return focTelemetryStatusResponseId;
    }

    void FocMotorCategoryClient::TelemetryStatusResponseMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < 6)
            return;

        FocTelemetryStatus status{
            static_cast<FocMotorState>(data[0]),
            static_cast<FocFaultCode>(data[1]),
            CanFrameCodec::ReadInt16(data, 2),
            CanFrameCodec::ReadInt16(data, 4)
        };

        parent.NotifyObservers([&status](auto& observer)
            {
                observer.OnTelemetryStatusResponse(status);
            });
    }
}
