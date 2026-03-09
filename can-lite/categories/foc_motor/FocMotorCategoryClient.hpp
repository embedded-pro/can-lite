#pragma once

#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageType.hpp"
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
    };

    class FocMotorCategoryClient
        : public CanCategoryClient
        , public infra::Subject<FocMotorCategoryClientObserver>
    {
    public:
        explicit FocMotorCategoryClient(CanFrameTransport& transport);

        uint8_t Id() const override;

        void SendQueryMotorType();
        void SendStart();
        void SendStop();
        void SendSetPidCurrent(const FocPidGains& gains);
        void SendSetPidSpeed(const FocPidGains& gains);
        void SendSetPidPosition(const FocPidGains& gains);
        void SendIdentifyElectrical();
        void SendIdentifyMechanical();
        void SendRequestTelemetry();
        void SendSetEncoderResolution(uint16_t resolution);

    private:
        void SendSimpleCommand(uint8_t messageTypeId);

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
        uint8_t sequenceCounter = 0;

        MotorTypeResponseMessageType motorTypeResponse;
        ElectricalParamsResponseMessageType electricalParamsResponse;
        MechanicalParamsResponseMessageType mechanicalParamsResponse;
        TelemetryElectricalResponseMessageType telemetryElectricalResponse;
        TelemetryStatusResponseMessageType telemetryStatusResponse;
    };
}
