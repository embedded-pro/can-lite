#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "examples/foc_motor/FocMotorDefinitions.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class FocMotorCategoryServer;

    class FocMotorCategoryServerObserver
        : public infra::SingleObserver<FocMotorCategoryServerObserver, FocMotorCategoryServer>
    {
    public:
        using infra::SingleObserver<FocMotorCategoryServerObserver, FocMotorCategoryServer>::SingleObserver;

        virtual void OnQueryMotorType(const infra::Function<void(FocMotorMode)>& onResult) = 0;
        virtual void OnStart(const infra::Function<void(CanAckStatus)>& onDone) = 0;
        virtual void OnStop(const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidCurrent(const FocPidGains& gains, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidSpeed(const FocPidGains& gains, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidPosition(const FocPidGains& gains, const infra::Function<void()>& onDone) = 0;
        virtual void OnIdentifyElectrical(const infra::Function<void(FocElectricalParams)>& onResult) = 0;
        virtual void OnIdentifyMechanical(const infra::Function<void(FocMechanicalParams)>& onResult) = 0;
        virtual void OnRequestTelemetry(const infra::Function<void(FocTelemetryElectrical, FocTelemetryStatus)>& onResult) = 0;
        virtual void OnSetEncoderResolution(uint16_t resolution, const infra::Function<void()>& onDone) = 0;
        virtual void OnSelectControlMode(FocMotorMode requestedMode, const infra::Function<void(FocMotorMode)>& onActivated) = 0;
        virtual void OnSetTorqueSetpoint(float value, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetSpeedSetpoint(float value, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPositionSetpoint(float value, const infra::Function<void()>& onDone) = 0;
        virtual void OnClearFault(const infra::Function<void()>& onDone) = 0;
        virtual void OnEmergencyStop(const infra::Function<void()>& onDone) = 0;
        virtual void OnConfigureTelemetryRate(uint8_t rateHz, const infra::Function<void()>& onDone) = 0;
    };

    class FocMotorCategoryServer
        : public CanCategoryServer
        , public infra::Subject<FocMotorCategoryServerObserver>
    {
    public:
        explicit FocMotorCategoryServer(CanFrameTransport& transport);

        uint8_t Id() const override;

        void SendCategoryError(uint8_t origCommandId, FocMotorCategoryError errorCode);
        void BroadcastFaultStatus(FocFaultCode fault);

    private:
        void SendMotorTypeResponse(FocMotorMode mode);
        void SendElectricalParamsResponse(const FocElectricalParams& params);
        void SendMechanicalParamsResponse(const FocMechanicalParams& params);
        void SendTelemetryElectricalResponse(const FocTelemetryElectrical& telemetry);
        void SendTelemetryStatusResponse(const FocTelemetryStatus& status);
        void SendSelectControlModeResponse(FocMotorMode activeMode);

        bool HandleQueryMotorType(const hal::Can::Message& data);
        bool HandleStart(const hal::Can::Message& data);
        bool HandleStop(const hal::Can::Message& data);
        bool HandleSetPidCurrent(const hal::Can::Message& data);
        bool HandleSetPidSpeed(const hal::Can::Message& data);
        bool HandleSetPidPosition(const hal::Can::Message& data);
        bool HandleIdentifyElectrical(const hal::Can::Message& data);
        bool HandleIdentifyMechanical(const hal::Can::Message& data);
        bool HandleRequestTelemetry(const hal::Can::Message& data);
        bool HandleSetEncoderResolution(const hal::Can::Message& data);
        bool HandleSelectControlMode(const hal::Can::Message& data);
        bool HandleSetTorqueSetpoint(const hal::Can::Message& data);
        bool HandleSetSpeedSetpoint(const hal::Can::Message& data);
        bool HandleSetPositionSetpoint(const hal::Can::Message& data);
        bool HandleClearFault(const hal::Can::Message& data);
        bool HandleEmergencyStop(const hal::Can::Message& data);
        bool HandleConfigureTelemetryRate(const hal::Can::Message& data);

        CanMessageHandler<FocMotorCategoryServer> queryMotorType{ focQueryMotorTypeId, *this, &FocMotorCategoryServer::HandleQueryMotorType };
        CanMessageHandler<FocMotorCategoryServer> start{ focStartId, *this, &FocMotorCategoryServer::HandleStart };
        CanMessageHandler<FocMotorCategoryServer> stop{ focStopId, *this, &FocMotorCategoryServer::HandleStop };
        CanMessageHandler<FocMotorCategoryServer> setPidCurrent{ focSetPidCurrentId, *this, &FocMotorCategoryServer::HandleSetPidCurrent };
        CanMessageHandler<FocMotorCategoryServer> setPidSpeed{ focSetPidSpeedId, *this, &FocMotorCategoryServer::HandleSetPidSpeed };
        CanMessageHandler<FocMotorCategoryServer> setPidPosition{ focSetPidPositionId, *this, &FocMotorCategoryServer::HandleSetPidPosition };
        CanMessageHandler<FocMotorCategoryServer> identifyElectrical{ focIdentifyElectricalId, *this, &FocMotorCategoryServer::HandleIdentifyElectrical };
        CanMessageHandler<FocMotorCategoryServer> identifyMechanical{ focIdentifyMechanicalId, *this, &FocMotorCategoryServer::HandleIdentifyMechanical };
        CanMessageHandler<FocMotorCategoryServer> requestTelemetry{ focRequestTelemetryId, *this, &FocMotorCategoryServer::HandleRequestTelemetry };
        CanMessageHandler<FocMotorCategoryServer> setEncoderResolution{ focSetEncoderResolutionId, *this, &FocMotorCategoryServer::HandleSetEncoderResolution };
        CanMessageHandler<FocMotorCategoryServer> selectControlMode{ focSelectControlModeId, *this, &FocMotorCategoryServer::HandleSelectControlMode };
        CanMessageHandler<FocMotorCategoryServer> setTorqueSetpoint{ focSetTorqueSetpointId, *this, &FocMotorCategoryServer::HandleSetTorqueSetpoint };
        CanMessageHandler<FocMotorCategoryServer> setSpeedSetpoint{ focSetSpeedSetpointId, *this, &FocMotorCategoryServer::HandleSetSpeedSetpoint };
        CanMessageHandler<FocMotorCategoryServer> setPositionSetpoint{ focSetPositionSetpointId, *this, &FocMotorCategoryServer::HandleSetPositionSetpoint };
        CanMessageHandler<FocMotorCategoryServer> clearFault{ focClearFaultId, *this, &FocMotorCategoryServer::HandleClearFault };
        CanMessageHandler<FocMotorCategoryServer> emergencyStop{ focEmergencyStopId, *this, &FocMotorCategoryServer::HandleEmergencyStop };
        CanMessageHandler<FocMotorCategoryServer> configureTelemetryRate{ focConfigureTelemetryRateId, *this, &FocMotorCategoryServer::HandleConfigureTelemetryRate };
    };
}
