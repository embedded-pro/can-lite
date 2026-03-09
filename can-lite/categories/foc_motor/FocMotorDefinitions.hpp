#pragma once

#include <cstdint>

namespace services
{
    static constexpr uint8_t focMotorCategoryId = 0x02;

    // Command message type IDs (Client → Server)
    static constexpr uint8_t focQueryMotorTypeId = 0x00;
    static constexpr uint8_t focStartId = 0x01;
    static constexpr uint8_t focStopId = 0x02;
    static constexpr uint8_t focSetPidCurrentId = 0x03;
    static constexpr uint8_t focSetPidSpeedId = 0x04;
    static constexpr uint8_t focSetPidPositionId = 0x05;
    static constexpr uint8_t focIdentifyElectricalId = 0x06;
    static constexpr uint8_t focIdentifyMechanicalId = 0x07;
    static constexpr uint8_t focRequestTelemetryId = 0x08;
    static constexpr uint8_t focSetEncoderResolutionId = 0x09;

    // Response message type IDs (Server → Client) = 0x80 + command ID
    static constexpr uint8_t focMotorTypeResponseId = 0x80;
    static constexpr uint8_t focElectricalParamsResponseId = 0x86;
    static constexpr uint8_t focMechanicalParamsResponseId = 0x87;
    static constexpr uint8_t focTelemetryElectricalResponseId = 0x88;
    static constexpr uint8_t focTelemetryStatusResponseId = 0x89;

    // Scale factors
    static constexpr int32_t focPidScale = 1;
    static constexpr int32_t focResistanceScale = 1000;
    static constexpr int32_t focInductanceScale = 1000;
    static constexpr int32_t focInertiaScale = 10000;
    static constexpr int32_t focFrictionScale = 10000;
    static constexpr int32_t focVoltageScale = 10;
    static constexpr int32_t focCurrentScale = 10;
    static constexpr int32_t focSpeedScale = 1;
    static constexpr int32_t focPositionScale = 100;

    enum class FocMotorMode : uint8_t
    {
        torque = 0,
        speed = 1,
        position = 2
    };

    enum class FocMotorState : uint8_t
    {
        idle = 0,
        running = 1,
        fault = 2,
        calibrating = 3
    };

    enum class FocFaultCode : uint8_t
    {
        none = 0,
        overCurrent = 1,
        overVoltage = 2,
        underVoltage = 3,
        overTemperature = 4,
        sensorFault = 5
    };

    struct FocPidGains
    {
        int16_t kp;
        int16_t ki;
        int16_t kd;
    };

    struct FocElectricalParams
    {
        int16_t resistance;
        int16_t inductance;
    };

    struct FocMechanicalParams
    {
        int16_t inertia;
        int16_t friction;
    };

    struct FocTelemetryElectrical
    {
        int16_t voltage;
        int16_t maxCurrent;
        int16_t iq;
        int16_t id;
    };

    struct FocTelemetryStatus
    {
        FocMotorState state;
        FocFaultCode fault;
        int16_t speed;
        int16_t position;
    };
}
