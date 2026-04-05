#pragma once

#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageType.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class CanProtocolClient;

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
    };

    class FocMotorCategoryClient
        : public CanCategoryClient
        , public infra::Subject<FocMotorCategoryClientObserver>
    {
    public:
        FocMotorCategoryClient(CanFrameTransport& transport, CanProtocolClient& client);

        uint8_t Id() const override;

        void SendQueryMotorType(uint16_t targetNodeId);
        void SendStart(uint16_t targetNodeId);
        void SendStop(uint16_t targetNodeId);
        void SendSetPidCurrent(uint16_t targetNodeId, const FocPidGains& gains);
        void SendSetPidSpeed(uint16_t targetNodeId, const FocPidGains& gains);
        void SendSetPidPosition(uint16_t targetNodeId, const FocPidGains& gains);
        void SendIdentifyElectrical(uint16_t targetNodeId);
        void SendIdentifyMechanical(uint16_t targetNodeId);
        void SendRequestTelemetry(uint16_t targetNodeId);
        void SendSetEncoderResolution(uint16_t targetNodeId, uint16_t resolution);
        void SendSetTarget(uint16_t targetNodeId, const FocSetpoint& setpoint);
        void SendClearFault(uint16_t targetNodeId);
        void SendEmergencyStop(uint16_t targetNodeId);
        void SendConfigureTelemetryRate(uint16_t targetNodeId, uint8_t rateHz);

    private:
        void SendSimpleCommand(uint16_t targetNodeId, uint8_t messageTypeId);

        class MotorTypeResponseMessageType
            : public CanMessageType
        {
        public:
            explicit MotorTypeResponseMessageType(FocMotorCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryClient& parent;
        };

        class ElectricalParamsResponseMessageType
            : public CanMessageType
        {
        public:
            explicit ElectricalParamsResponseMessageType(FocMotorCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryClient& parent;
        };

        class MechanicalParamsResponseMessageType
            : public CanMessageType
        {
        public:
            explicit MechanicalParamsResponseMessageType(FocMotorCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryClient& parent;
        };

        class TelemetryElectricalResponseMessageType
            : public CanMessageType
        {
        public:
            explicit TelemetryElectricalResponseMessageType(FocMotorCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryClient& parent;
        };

        class TelemetryStatusResponseMessageType
            : public CanMessageType
        {
        public:
            explicit TelemetryStatusResponseMessageType(FocMotorCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryClient& parent;
        };

        CanFrameTransport& transport;
        CanProtocolClient& client;

        MotorTypeResponseMessageType motorTypeResponse;
        ElectricalParamsResponseMessageType electricalParamsResponse;
        MechanicalParamsResponseMessageType mechanicalParamsResponse;
        TelemetryElectricalResponseMessageType telemetryElectricalResponse;
        TelemetryStatusResponseMessageType telemetryStatusResponse;
    };
}
