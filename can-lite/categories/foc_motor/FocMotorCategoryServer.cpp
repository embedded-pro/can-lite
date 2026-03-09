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
        parent.NotifyObservers([](auto& observer)
            {
                observer.OnQueryMotorType();
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
        parent.NotifyObservers([](auto& observer)
            {
                observer.OnStart();
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
        parent.NotifyObservers([](auto& observer)
            {
                observer.OnStop();
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
            return;

        FocPidGains gains{
            CanFrameCodec::ReadInt16(data, 1),
            CanFrameCodec::ReadInt16(data, 3),
            CanFrameCodec::ReadInt16(data, 5)
        };

        parent.NotifyObservers([&gains](auto& observer)
            {
                observer.OnSetPidCurrent(gains);
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
            return;

        FocPidGains gains{
            CanFrameCodec::ReadInt16(data, 1),
            CanFrameCodec::ReadInt16(data, 3),
            CanFrameCodec::ReadInt16(data, 5)
        };

        parent.NotifyObservers([&gains](auto& observer)
            {
                observer.OnSetPidSpeed(gains);
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
            return;

        FocPidGains gains{
            CanFrameCodec::ReadInt16(data, 1),
            CanFrameCodec::ReadInt16(data, 3),
            CanFrameCodec::ReadInt16(data, 5)
        };

        parent.NotifyObservers([&gains](auto& observer)
            {
                observer.OnSetPidPosition(gains);
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
        parent.NotifyObservers([](auto& observer)
            {
                observer.OnIdentifyElectrical();
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
        parent.NotifyObservers([](auto& observer)
            {
                observer.OnIdentifyMechanical();
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
        parent.NotifyObservers([](auto& observer)
            {
                observer.OnRequestTelemetry();
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
            return;

        auto resolution = static_cast<uint16_t>(CanFrameCodec::ReadInt16(data, 1));

        parent.NotifyObservers([resolution](auto& observer)
            {
                observer.OnSetEncoderResolution(resolution);
            });
    }
}
