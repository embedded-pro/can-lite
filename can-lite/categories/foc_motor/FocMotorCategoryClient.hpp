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
        virtual void OnSelectControlModeResponse(FocMotorMode activeMode, FocRejectReason result) = 0;
        virtual void OnCommandRejected(uint8_t origCmdId, FocRejectReason reason) = 0;
    };

    class FocMotorCategoryClient
        : public CanCategoryClient
        , public infra::Subject<FocMotorCategoryClientObserver>
    {
    public:
        FocMotorCategoryClient(CanFrameTransport& transport, CanProtocolClient& client);

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
        bool SendSimpleCommand(uint16_t targetNodeId, uint8_t messageTypeId);

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

        class SelectControlModeResponseMessageType
            : public CanMessageType
        {
        public:
            explicit SelectControlModeResponseMessageType(FocMotorCategoryClient& parent);
            uint8_t Id() const override;
            void Handle(const hal::Can::Message& data) override;

        private:
            FocMotorCategoryClient& parent;
        };

        class CommandRejectedResponseMessageType
            : public CanMessageType
        {
        public:
            explicit CommandRejectedResponseMessageType(FocMotorCategoryClient& parent);
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
        SelectControlModeResponseMessageType selectControlModeResponse;
        CommandRejectedResponseMessageType commandRejectedResponse;
    };
}
