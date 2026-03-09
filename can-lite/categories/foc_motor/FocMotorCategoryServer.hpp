#pragma once

#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageType.hpp"
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

        virtual void OnQueryMotorType() = 0;
        virtual void OnStart() = 0;
        virtual void OnStop() = 0;
        virtual void OnSetPidCurrent(const FocPidGains& gains) = 0;
        virtual void OnSetPidSpeed(const FocPidGains& gains) = 0;
        virtual void OnSetPidPosition(const FocPidGains& gains) = 0;
        virtual void OnIdentifyElectrical() = 0;
        virtual void OnIdentifyMechanical() = 0;
        virtual void OnRequestTelemetry() = 0;
        virtual void OnSetEncoderResolution(uint16_t resolution) = 0;
    };

    class FocMotorCategoryServer
        : public CanCategoryServer
        , public infra::Subject<FocMotorCategoryServerObserver>
    {
    public:
        explicit FocMotorCategoryServer(CanFrameTransport& transport);

        uint8_t Id() const override;

        void SendMotorTypeResponse(FocMotorMode mode);
        void SendElectricalParamsResponse(const FocElectricalParams& params);
        void SendMechanicalParamsResponse(const FocMechanicalParams& params);
        void SendTelemetryElectricalResponse(const FocTelemetryElectrical& telemetry);
        void SendTelemetryStatusResponse(const FocTelemetryStatus& status);

    private:
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
    };
}
