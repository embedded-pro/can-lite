#pragma once

#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageType.hpp"
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
        virtual void OnStart(const infra::Function<void()>& onDone) = 0;
        virtual void OnStop(const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidCurrent(const FocPidGains& gains, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidSpeed(const FocPidGains& gains, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidPosition(const FocPidGains& gains, const infra::Function<void()>& onDone) = 0;
        virtual void OnIdentifyElectrical(const infra::Function<void(FocElectricalParams)>& onResult) = 0;
        virtual void OnIdentifyMechanical(const infra::Function<void(FocMechanicalParams)>& onResult) = 0;
        virtual void OnRequestTelemetry(const infra::Function<void(FocTelemetryElectrical, FocTelemetryStatus)>& onResult) = 0;
        virtual void OnSetEncoderResolution(uint16_t resolution, const infra::Function<void()>& onDone) = 0;
        virtual void OnSelectControlMode(FocMotorMode requestedMode, const infra::Function<void(FocMotorMode)>& onActivated) = 0;
        virtual void OnSetTorqueSetpoint(int16_t value, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetSpeedSetpoint(int16_t value, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPositionSetpoint(int16_t value, const infra::Function<void()>& onDone) = 0;
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

        class QueryMotorTypeMessageType
            : public CanMessageType
        {
        public:
            explicit QueryMotorTypeMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class StartMessageType
            : public CanMessageType
        {
        public:
            explicit StartMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class StopMessageType
            : public CanMessageType
        {
        public:
            explicit StopMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class SetPidCurrentMessageType
            : public CanMessageType
        {
        public:
            explicit SetPidCurrentMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class SetPidSpeedMessageType
            : public CanMessageType
        {
        public:
            explicit SetPidSpeedMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class SetPidPositionMessageType
            : public CanMessageType
        {
        public:
            explicit SetPidPositionMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class IdentifyElectricalMessageType
            : public CanMessageType
        {
        public:
            explicit IdentifyElectricalMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class IdentifyMechanicalMessageType
            : public CanMessageType
        {
        public:
            explicit IdentifyMechanicalMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class RequestTelemetryMessageType
            : public CanMessageType
        {
        public:
            explicit RequestTelemetryMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class SetEncoderResolutionMessageType
            : public CanMessageType
        {
        public:
            explicit SetEncoderResolutionMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class SelectControlModeMessageType
            : public CanMessageType
        {
        public:
            explicit SelectControlModeMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class SetTorqueSetpointMessageType
            : public CanMessageType
        {
        public:
            explicit SetTorqueSetpointMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class SetSpeedSetpointMessageType
            : public CanMessageType
        {
        public:
            explicit SetSpeedSetpointMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class SetPositionSetpointMessageType
            : public CanMessageType
        {
        public:
            explicit SetPositionSetpointMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class ClearFaultMessageType
            : public CanMessageType
        {
        public:
            explicit ClearFaultMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class EmergencyStopMessageType
            : public CanMessageType
        {
        public:
            explicit EmergencyStopMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        class ConfigureTelemetryRateMessageType
            : public CanMessageType
        {
        public:
            explicit ConfigureTelemetryRateMessageType(FocMotorCategoryServer& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryServer& parent;
        };

        CanFrameTransport& transport;

        QueryMotorTypeMessageType queryMotorType;
        StartMessageType start;
        StopMessageType stop;
        SetPidCurrentMessageType setPidCurrent;
        SetPidSpeedMessageType setPidSpeed;
        SetPidPositionMessageType setPidPosition;
        IdentifyElectricalMessageType identifyElectrical;
        IdentifyMechanicalMessageType identifyMechanical;
        RequestTelemetryMessageType requestTelemetry;
        SetEncoderResolutionMessageType setEncoderResolution;
        SelectControlModeMessageType selectControlMode;
        SetTorqueSetpointMessageType setTorqueSetpoint;
        SetSpeedSetpointMessageType setSpeedSetpoint;
        SetPositionSetpointMessageType setPositionSetpoint;
        ClearFaultMessageType clearFault;
        EmergencyStopMessageType emergencyStop;
        ConfigureTelemetryRateMessageType configureTelemetryRate;
    };
}
