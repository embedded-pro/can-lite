#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "examples/foc_motor/FocMotorDefinitions.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class FocMotorCategoryClient;

    class FocMotorCategoryClientObserver
        : public infra::SingleObserver<FocMotorCategoryClientObserver, FocMotorCategoryClient>
    {
    public:
        using infra::SingleObserver<FocMotorCategoryClientObserver, FocMotorCategoryClient>::SingleObserver;

        virtual void OnMotorTypeResponse(FocMotorMode mode) = 0;
        virtual void OnElectricalParamsResponse(const FocElectricalParams& params) = 0;
        virtual void OnMechanicalParamsResponse(const FocMechanicalParams& params) = 0;
        virtual void OnTelemetryElectricalResponse(const FocTelemetryElectrical& telemetry) = 0;
        virtual void OnTelemetryStatusResponse(const FocTelemetryStatus& status) = 0;
        virtual void OnSelectControlModeResponse(FocMotorMode activeMode) = 0;
    };

    class FocMotorCategoryClient
        : public CanCategoryClient
        , public infra::Subject<FocMotorCategoryClientObserver>
    {
    public:
        FocMotorCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource);

        uint8_t Id() const override;

        bool SendQueryMotorType(uint16_t targetNodeId);
        bool SendStart(uint16_t targetNodeId);
        bool SendStop(uint16_t targetNodeId);
        bool SendSetPidCurrent(uint16_t targetNodeId, const FocPidGains& gains);
        bool SendSetPidSpeed(uint16_t targetNodeId, const FocPidGains& gains);
        bool SendSetPidPosition(uint16_t targetNodeId, const FocPidGains& gains);
        bool SendIdentifyElectrical(uint16_t targetNodeId);
        bool SendIdentifyMechanical(uint16_t targetNodeId);
        bool SendRequestTelemetry(uint16_t targetNodeId);
        bool SendSetEncoderResolution(uint16_t targetNodeId, uint16_t resolution);
        bool SendSelectControlMode(uint16_t targetNodeId, FocMotorMode mode);
        bool SendSetTorqueSetpoint(uint16_t targetNodeId, int16_t value);
        bool SendSetSpeedSetpoint(uint16_t targetNodeId, int16_t value);
        bool SendSetPositionSetpoint(uint16_t targetNodeId, int16_t value);
        bool SendClearFault(uint16_t targetNodeId);
        bool SendEmergencyStop(uint16_t targetNodeId);
        bool SendConfigureTelemetryRate(uint16_t targetNodeId, uint8_t rateHz);

    private:
        void HandleMotorTypeResponse(const hal::Can::Message& data);
        void HandleElectricalParamsResponse(const hal::Can::Message& data);
        void HandleMechanicalParamsResponse(const hal::Can::Message& data);
        void HandleTelemetryElectricalResponse(const hal::Can::Message& data);
        void HandleTelemetryStatusResponse(const hal::Can::Message& data);
        void HandleSelectControlModeResponse(const hal::Can::Message& data);

        CanMessageHandler<FocMotorCategoryClient> motorTypeResponse{ focMotorTypeResponseId, *this, &FocMotorCategoryClient::HandleMotorTypeResponse };
        CanMessageHandler<FocMotorCategoryClient> electricalParamsResponse{ focElectricalParamsResponseId, *this, &FocMotorCategoryClient::HandleElectricalParamsResponse };
        CanMessageHandler<FocMotorCategoryClient> mechanicalParamsResponse{ focMechanicalParamsResponseId, *this, &FocMotorCategoryClient::HandleMechanicalParamsResponse };
        CanMessageHandler<FocMotorCategoryClient> telemetryElectricalResponse{ focTelemetryElectricalResponseId, *this, &FocMotorCategoryClient::HandleTelemetryElectricalResponse };
        CanMessageHandler<FocMotorCategoryClient> telemetryStatusResponse{ focTelemetryStatusResponseId, *this, &FocMotorCategoryClient::HandleTelemetryStatusResponse };
        CanMessageHandler<FocMotorCategoryClient> selectControlModeResponse{ focSelectControlModeResponseId, *this, &FocMotorCategoryClient::HandleSelectControlModeResponse };
    };
}
