#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanCommandAcknowledgerMock.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "examples/foc_motor/FocMotorCategoryServer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::ElementsAreArray;
    using testing::FloatEq;
    using testing::Invoke;
    using testing::StrictMock;

    class FocMotorCategoryServerObserverMock
        : public FocMotorCategoryServerObserver
    {
    public:
        using FocMotorCategoryServerObserver::FocMotorCategoryServerObserver;

        MOCK_METHOD(void, OnQueryMotorType, (const infra::Function<void(FocMotorMode)>& onResult), (override));
        MOCK_METHOD(void, OnStart, (const infra::Function<void(CanAckStatus)>& onDone), (override));
        MOCK_METHOD(void, OnStop, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPidCurrent, (const FocPidGains& gains, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPidSpeed, (const FocPidGains& gains, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPidPosition, (const FocPidGains& gains, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnIdentifyElectrical, (const infra::Function<void(FocElectricalParams)>& onResult), (override));
        MOCK_METHOD(void, OnIdentifyMechanical, (const infra::Function<void(FocMechanicalParams)>& onResult), (override));
        MOCK_METHOD(void, OnRequestTelemetry, (const infra::Function<void(FocTelemetryElectrical, FocTelemetryStatus)>& onResult), (override));
        MOCK_METHOD(void, OnSetEncoderResolution, (uint16_t resolution, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSelectControlMode, (FocMotorMode mode, const infra::Function<void(FocMotorMode)>& onActivated), (override));
        MOCK_METHOD(void, OnSetTorqueSetpoint, (float value, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetSpeedSetpoint, (float value, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPositionSetpoint, (float value, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnClearFault, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnEmergencyStop, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnConfigureTelemetryRate, (uint8_t rateHz, const infra::Function<void()>& onDone), (override));
    };

    class TestFocMotorCategoryServer : public ::testing::Test
    {
    public:
        TestFocMotorCategoryServer()
        {
            server.SetAcknowledger(acknowledger);
        }

        void ExpectResponseFrame(uint8_t messageType, std::initializer_list<uint8_t> bytes, uint16_t nodeId = 1)
        {
            EXPECT_CALL(canMock, SendData(
                                     hal::Can::Id::Create29BitId(MakeCanId(CanPriority::response, focMotorCategoryId, messageType, nodeId)),
                                     ElementsAreArray(bytes),
                                     testing::_))
                .WillOnce([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    });
        }

        void ExpectTelemetryFrame(uint8_t messageType, std::initializer_list<uint8_t> bytes, uint16_t nodeId = 1)
        {
            EXPECT_CALL(canMock, SendData(
                                     hal::Can::Id::Create29BitId(MakeCanId(CanPriority::telemetry, focMotorCategoryId, messageType, nodeId)),
                                     ElementsAreArray(bytes),
                                     testing::_))
                .WillOnce([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    });
        }

        void ExpectAck(uint8_t commandId, CanAckStatus status = CanAckStatus::success)
        {
            EXPECT_CALL(acknowledger, SendCommandAck(focMotorCategoryId, commandId, status));
        }

        StrictMock<hal::CanMock> canMock;
        CanFrameTransport transport{ canMock, 1 };
        FocMotorCategoryServer server{ transport };
        StrictMock<CanCommandAcknowledgerMock> acknowledger;
    };

    class TestFocMotorCategoryServerWithObserver : public TestFocMotorCategoryServer
    {
    public:
        StrictMock<FocMotorCategoryServerObserverMock> observer{ server };
    };

    TEST_F(TestFocMotorCategoryServer, Id)
    {
        EXPECT_EQ(server.Id(), focMotorCategoryId);
    }

    TEST_F(TestFocMotorCategoryServer, RequiresSequenceValidation)
    {
        EXPECT_TRUE(server.RequiresSequenceValidation());
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, QueryMotorType_CallbackSendsResponseAndAck)
    {
        EXPECT_CALL(observer, OnQueryMotorType(_)).WillOnce(Invoke([](const infra::Function<void(FocMotorMode)>& cb)
            {
                cb(FocMotorMode::speed);
            }));
        ExpectResponseFrame(focMotorTypeResponseId, { static_cast<uint8_t>(FocMotorMode::speed) });
        ExpectAck(focQueryMotorTypeId);

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focQueryMotorTypeId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, Start_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnStart(_)).WillOnce(Invoke([](const infra::Function<void(CanAckStatus)>& cb)
            {
                cb(CanAckStatus::success);
            }));
        ExpectAck(focStartId);

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focStartId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, Start_CallbackCanReturnInvalidState)
    {
        EXPECT_CALL(observer, OnStart(_)).WillOnce(Invoke([](const infra::Function<void(CanAckStatus)>& cb)
            {
                cb(CanAckStatus::invalidState);
            }));
        ExpectAck(focStartId, CanAckStatus::invalidState);

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focStartId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, Stop_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnStop(_)).WillOnce(Invoke([](const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectAck(focStopId);

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focStopId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPidCurrent_ParsesGains)
    {
        EXPECT_CALL(observer, OnSetPidCurrent(_, _)).WillOnce(Invoke([](const FocPidGains& gains, const infra::Function<void()>& cb)
            {
                EXPECT_EQ(gains.kp, 100);
                EXPECT_EQ(gains.ki, 200);
                EXPECT_EQ(gains.kd, 300);
                cb();
            }));
        ExpectAck(focSetPidCurrentId);

        hal::Can::Message data;
        data.resize(7, 0);
        CanFrameCodec::WriteInt16(data, 1, 100);
        CanFrameCodec::WriteInt16(data, 3, 200);
        CanFrameCodec::WriteInt16(data, 5, 300);
        server.HandleMessage(focSetPidCurrentId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidCurrent_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        EXPECT_EQ(server.HandleMessage(focSetPidCurrentId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPidSpeed_ParsesGains)
    {
        EXPECT_CALL(observer, OnSetPidSpeed(_, _)).WillOnce(Invoke([](const FocPidGains& gains, const infra::Function<void()>& cb)
            {
                EXPECT_EQ(gains.kp, -500);
                cb();
            }));
        ExpectAck(focSetPidSpeedId);

        hal::Can::Message data;
        data.resize(7, 0);
        CanFrameCodec::WriteInt16(data, 1, -500);
        server.HandleMessage(focSetPidSpeedId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidSpeed_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        EXPECT_EQ(server.HandleMessage(focSetPidSpeedId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPidPosition_ParsesGains)
    {
        EXPECT_CALL(observer, OnSetPidPosition(_, _)).WillOnce(Invoke([](const FocPidGains& gains, const infra::Function<void()>& cb)
            {
                EXPECT_EQ(gains.kp, 10);
                cb();
            }));
        ExpectAck(focSetPidPositionId);

        hal::Can::Message data;
        data.resize(7, 0);
        CanFrameCodec::WriteInt16(data, 1, 10);
        server.HandleMessage(focSetPidPositionId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidPosition_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        EXPECT_EQ(server.HandleMessage(focSetPidPositionId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, IdentifyElectrical_CallbackSendsResponse)
    {
        EXPECT_CALL(observer, OnIdentifyElectrical(_)).WillOnce(Invoke([](const infra::Function<void(FocElectricalParams)>& cb)
            {
                cb(FocElectricalParams{ 1.5f, 0.8f });
            }));
        ExpectResponseFrame(focElectricalParamsResponseId, { 0x05, 0xDC, 0x03, 0x20 });
        ExpectAck(focIdentifyElectricalId);

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focIdentifyElectricalId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, IdentifyMechanical_CallbackSendsResponse)
    {
        EXPECT_CALL(observer, OnIdentifyMechanical(_)).WillOnce(Invoke([](const infra::Function<void(FocMechanicalParams)>& cb)
            {
                cb(FocMechanicalParams{ 0.5f, 0.2f });
            }));
        ExpectResponseFrame(focMechanicalParamsResponseId, { 0x13, 0x88, 0x07, 0xD0 });
        ExpectAck(focIdentifyMechanicalId);

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focIdentifyMechanicalId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, RequestTelemetry_CallbackSendsBothFrames)
    {
        EXPECT_CALL(observer, OnRequestTelemetry(_)).WillOnce(Invoke([](const infra::Function<void(FocTelemetryElectrical, FocTelemetryStatus)>& cb)
            {
                cb(FocTelemetryElectrical{ 24.0f, 10.0f, -5.0f, 2.5f },
                    FocTelemetryStatus{ FocMotorState::running, FocFaultCode::none, 3000.0f, 18.0f });
            }));
        ExpectTelemetryFrame(focTelemetryElectricalResponseId,
            { 0x00, 0xF0, 0x00, 0x64, 0xFF, 0xCE, 0x00, 0x19 });
        ExpectTelemetryFrame(focTelemetryStatusResponseId,
            { static_cast<uint8_t>(FocMotorState::running), static_cast<uint8_t>(FocFaultCode::none),
                0x0B, 0xB8, 0x07, 0x08 });
        ExpectAck(focRequestTelemetryId);

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focRequestTelemetryId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetEncoderResolution_ParsesResolution)
    {
        EXPECT_CALL(observer, OnSetEncoderResolution(4096, _)).WillOnce(Invoke([](uint16_t, const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectAck(focSetEncoderResolutionId);

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 4096);
        server.HandleMessage(focSetEncoderResolutionId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetEncoderResolution_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        EXPECT_EQ(server.HandleMessage(focSetEncoderResolutionId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServer, UnknownMessageType_ReturnsFalse)
    {
        EXPECT_EQ(server.HandleMessage(0xFF, hal::Can::Message{}), CanDispatchResult::unknownMessageType);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SelectControlMode_CallbackSendsResponseAndAck)
    {
        EXPECT_CALL(observer, OnSelectControlMode(FocMotorMode::speed, _)).WillOnce(Invoke([](FocMotorMode, const infra::Function<void(FocMotorMode)>& cb)
            {
                cb(FocMotorMode::speed);
            }));
        ExpectResponseFrame(focSelectControlModeResponseId, { static_cast<uint8_t>(FocMotorMode::speed) });
        ExpectAck(focSelectControlModeId);

        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = static_cast<uint8_t>(FocMotorMode::speed);
        server.HandleMessage(focSelectControlModeId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetTorqueSetpoint_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnSetTorqueSetpoint(FloatEq(50.0f), _)).WillOnce(Invoke([](float, const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectAck(focSetTorqueSetpointId);

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 500);
        server.HandleMessage(focSetTorqueSetpointId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetTorqueSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        EXPECT_EQ(server.HandleMessage(focSetTorqueSetpointId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServer, SelectControlMode_InvalidModeRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = 0xFF;
        EXPECT_EQ(server.HandleMessage(focSelectControlModeId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, ClearFault_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnClearFault(_)).WillOnce(Invoke([](const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectAck(focClearFaultId);

        hal::Can::Message data;
        data.push_back(0);
        server.HandleMessage(focClearFaultId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, EmergencyStop_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnEmergencyStop(_)).WillOnce(Invoke([](const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectAck(focEmergencyStopId);

        hal::Can::Message data;
        data.push_back(0);
        server.HandleMessage(focEmergencyStopId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, ConfigureTelemetryRate_ParsesRate)
    {
        EXPECT_CALL(observer, OnConfigureTelemetryRate(10u, _)).WillOnce(Invoke([](uint8_t, const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectAck(focConfigureTelemetryRateId);

        hal::Can::Message data;
        data.push_back(0);
        data.push_back(10);
        server.HandleMessage(focConfigureTelemetryRateId, data);
    }

    TEST_F(TestFocMotorCategoryServer, ConfigureTelemetryRate_TooShortRejected)
    {
        hal::Can::Message data;
        data.push_back(0);
        EXPECT_EQ(server.HandleMessage(focConfigureTelemetryRateId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServer, SelectControlMode_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(1, 0);
        EXPECT_EQ(server.HandleMessage(focSelectControlModeId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetSpeedSetpoint_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnSetSpeedSetpoint(FloatEq(1500.0f), _)).WillOnce(Invoke([](float, const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectAck(focSetSpeedSetpointId);

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 1500);
        server.HandleMessage(focSetSpeedSetpointId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetSpeedSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        EXPECT_EQ(server.HandleMessage(focSetSpeedSetpointId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetSpeedSetpoint_32000RpmDecodesCorrectly)
    {
        EXPECT_CALL(observer, OnSetSpeedSetpoint(FloatEq(32000.0f), _)).WillOnce(Invoke([](float, const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectAck(focSetSpeedSetpointId);

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 32000);
        server.HandleMessage(focSetSpeedSetpointId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPositionSetpoint_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnSetPositionSetpoint(FloatEq(-180.0f), _)).WillOnce(Invoke([](float, const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectAck(focSetPositionSetpointId);

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, -18000);
        server.HandleMessage(focSetPositionSetpointId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetPositionSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        EXPECT_EQ(server.HandleMessage(focSetPositionSetpointId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFocMotorCategoryServer, SendCategoryError_EmitsFrameAndAck)
    {
        ExpectResponseFrame(canCategoryErrorResponseMessageTypeId,
            { focSelectControlModeId, static_cast<uint8_t>(FocMotorCategoryError::modeMismatch) });
        ExpectAck(focSelectControlModeId, CanAckStatus::categoryError);

        server.SendCategoryError(focSelectControlModeId, FocMotorCategoryError::modeMismatch);
    }

    TEST_F(TestFocMotorCategoryServer, BroadcastFaultStatus_EmitsTelemetryStatusFrame)
    {
        ExpectTelemetryFrame(focTelemetryStatusResponseId,
            { static_cast<uint8_t>(FocMotorState::fault), static_cast<uint8_t>(FocFaultCode::overCurrent),
                0x00, 0x00, 0x00, 0x00 });

        server.BroadcastFaultStatus(FocFaultCode::overCurrent);
    }
}
