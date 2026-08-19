#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "examples/foc_motor/FocMotorCategoryServer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::AnyNumber;
    using testing::FloatEq;
    using testing::Invoke;
    using testing::StrictMock;

    class CanCommandAcknowledgerMock
        : public CanCommandAcknowledger
    {
    public:
        MOCK_METHOD(void, SendCommandAck, (uint8_t categoryId, uint8_t commandType, CanAckStatus status), (override));
    };

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
            EXPECT_CALL(canMock, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
                {
                    lastSentId = id;
                    lastSentData = data;
                    sendCount++;
                    cb(true);
                }));
            EXPECT_CALL(acknowledger, SendCommandAck(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([this](uint8_t catId, uint8_t cmdType, CanAckStatus status)
                {
                    lastAckCategoryId = catId;
                    lastAckCommandType = cmdType;
                    lastAckStatus = status;
                    ++ackCount;
                }));
            server.SetAcknowledger(acknowledger);
        }

        StrictMock<hal::CanMock> canMock;
        CanFrameTransport transport{ canMock, 1 };
        FocMotorCategoryServer server{ transport };
        StrictMock<CanCommandAcknowledgerMock> acknowledger;
        hal::Can::Id lastSentId{ hal::Can::Id::Create29BitId(0) };
        hal::Can::Message lastSentData;
        std::size_t sendCount{ 0 };
        uint8_t lastAckCategoryId{ 0 };
        uint8_t lastAckCommandType{ 0 };
        CanAckStatus lastAckStatus{ CanAckStatus::success };
        std::size_t ackCount{ 0 };
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

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focQueryMotorTypeId, data);

        EXPECT_EQ(ExtractCanMessageType(lastSentId.Get29BitId()), focMotorTypeResponseId);
        ASSERT_EQ(lastSentData.size(), 1u);
        EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(FocMotorMode::speed));
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
        EXPECT_EQ(lastAckCommandType, focQueryMotorTypeId);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, Start_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnStart(_)).WillOnce(Invoke([](const infra::Function<void(CanAckStatus)>& cb)
            {
                cb(CanAckStatus::success);
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focStartId, data);

        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
        EXPECT_EQ(lastAckCommandType, focStartId);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, Start_CallbackCanReturnInvalidState)
    {
        EXPECT_CALL(observer, OnStart(_)).WillOnce(Invoke([](const infra::Function<void(CanAckStatus)>& cb)
            {
                cb(CanAckStatus::invalidState);
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focStartId, data);

        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidState);
        EXPECT_EQ(lastAckCommandType, focStartId);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, Stop_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnStop(_)).WillOnce(Invoke([](const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focStopId, data);

        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
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

        hal::Can::Message data;
        data.resize(7, 0);
        CanFrameCodec::WriteInt16(data, 1, 100);
        CanFrameCodec::WriteInt16(data, 3, 200);
        CanFrameCodec::WriteInt16(data, 5, 300);
        server.HandleMessage(focSetPidCurrentId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidCurrent_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        server.HandleMessage(focSetPidCurrentId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPidSpeed_ParsesGains)
    {
        EXPECT_CALL(observer, OnSetPidSpeed(_, _)).WillOnce(Invoke([](const FocPidGains& gains, const infra::Function<void()>& cb)
            {
                EXPECT_EQ(gains.kp, -500);
                cb();
            }));

        hal::Can::Message data;
        data.resize(7, 0);
        CanFrameCodec::WriteInt16(data, 1, -500);
        server.HandleMessage(focSetPidSpeedId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidSpeed_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        server.HandleMessage(focSetPidSpeedId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPidPosition_ParsesGains)
    {
        EXPECT_CALL(observer, OnSetPidPosition(_, _)).WillOnce(Invoke([](const FocPidGains& gains, const infra::Function<void()>& cb)
            {
                EXPECT_EQ(gains.kp, 10);
                cb();
            }));

        hal::Can::Message data;
        data.resize(7, 0);
        CanFrameCodec::WriteInt16(data, 1, 10);
        server.HandleMessage(focSetPidPositionId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidPosition_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        server.HandleMessage(focSetPidPositionId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, IdentifyElectrical_CallbackSendsResponse)
    {
        EXPECT_CALL(observer, OnIdentifyElectrical(_)).WillOnce(Invoke([](const infra::Function<void(FocElectricalParams)>& cb)
            {
                cb(FocElectricalParams{ 1.5f, 0.8f });
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focIdentifyElectricalId, data);

        EXPECT_EQ(ExtractCanMessageType(lastSentId.Get29BitId()), focElectricalParamsResponseId);
        ASSERT_EQ(lastSentData.size(), 4u);
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 0), 1500);
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 2), 800);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, IdentifyMechanical_CallbackSendsResponse)
    {
        EXPECT_CALL(observer, OnIdentifyMechanical(_)).WillOnce(Invoke([](const infra::Function<void(FocMechanicalParams)>& cb)
            {
                cb(FocMechanicalParams{ 0.5f, 0.2f });
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focIdentifyMechanicalId, data);

        EXPECT_EQ(ExtractCanMessageType(lastSentId.Get29BitId()), focMechanicalParamsResponseId);
        ASSERT_EQ(lastSentData.size(), 4u);
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 0), 5000);
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 2), 2000);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, RequestTelemetry_CallbackSendsBothFrames)
    {
        EXPECT_CALL(observer, OnRequestTelemetry(_)).WillOnce(Invoke([](const infra::Function<void(FocTelemetryElectrical, FocTelemetryStatus)>& cb)
            {
                cb(FocTelemetryElectrical{ 24.0f, 10.0f, -5.0f, 2.5f },
                    FocTelemetryStatus{ FocMotorState::running, FocFaultCode::none, 3000.0f, 18.0f });
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focRequestTelemetryId, data);

        EXPECT_EQ(sendCount, 2u);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetEncoderResolution_ParsesResolution)
    {
        EXPECT_CALL(observer, OnSetEncoderResolution(4096, _)).WillOnce(Invoke([](uint16_t, const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 4096);
        server.HandleMessage(focSetEncoderResolutionId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetEncoderResolution_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        server.HandleMessage(focSetEncoderResolutionId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServer, UnknownMessageType_ReturnsFalse)
    {
        EXPECT_FALSE(server.HandleMessage(0xFF, hal::Can::Message{}));
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SelectControlMode_CallbackSendsResponseAndAck)
    {
        EXPECT_CALL(observer, OnSelectControlMode(FocMotorMode::speed, _)).WillOnce(Invoke([](FocMotorMode, const infra::Function<void(FocMotorMode)>& cb)
            {
                cb(FocMotorMode::speed);
            }));

        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = static_cast<uint8_t>(FocMotorMode::speed);
        server.HandleMessage(focSelectControlModeId, data);

        EXPECT_EQ(ExtractCanMessageType(lastSentId.Get29BitId()), focSelectControlModeResponseId);
        ASSERT_EQ(lastSentData.size(), 1u);
        EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(FocMotorMode::speed));
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetTorqueSetpoint_CallbackSendsAck)
    {
        // Wire: raw int16 = 500; ReadFixed16(focCurrentScale=10) = 50.0f A
        EXPECT_CALL(observer, OnSetTorqueSetpoint(FloatEq(50.0f), _)).WillOnce(Invoke([](float, const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 500);
        server.HandleMessage(focSetTorqueSetpointId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetTorqueSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        server.HandleMessage(focSetTorqueSetpointId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServer, SelectControlMode_InvalidModeRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = 0xFF;
        server.HandleMessage(focSelectControlModeId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, ClearFault_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnClearFault(_)).WillOnce(Invoke([](const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.push_back(0);
        server.HandleMessage(focClearFaultId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, EmergencyStop_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnEmergencyStop(_)).WillOnce(Invoke([](const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.push_back(0);
        server.HandleMessage(focEmergencyStopId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, ConfigureTelemetryRate_ParsesRate)
    {
        EXPECT_CALL(observer, OnConfigureTelemetryRate(10u, _)).WillOnce(Invoke([](uint8_t, const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.push_back(0);
        data.push_back(10);
        server.HandleMessage(focConfigureTelemetryRateId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, ConfigureTelemetryRate_TooShortRejected)
    {
        hal::Can::Message data;
        data.push_back(0);
        server.HandleMessage(focConfigureTelemetryRateId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServer, SelectControlMode_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(1, 0);
        server.HandleMessage(focSelectControlModeId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetSpeedSetpoint_CallbackSendsAck)
    {
        // Wire: raw int16 = 1500; ReadFixed16(focSpeedScale=1) = 1500.0f RPM
        EXPECT_CALL(observer, OnSetSpeedSetpoint(FloatEq(1500.0f), _)).WillOnce(Invoke([](float, const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 1500);
        server.HandleMessage(focSetSpeedSetpointId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetSpeedSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        server.HandleMessage(focSetSpeedSetpointId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetSpeedSetpoint_32000RpmDecodesCorrectly)
    {
        // Regression: at old focSpeedScale=10, WriteFixed16(32000,10)=32767 (saturated).
        // At corrected scale=1, raw 32000 decodes exactly to 32000.0f RPM.
        EXPECT_CALL(observer, OnSetSpeedSetpoint(FloatEq(32000.0f), _)).WillOnce(Invoke([](float, const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 32000);
        server.HandleMessage(focSetSpeedSetpointId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPositionSetpoint_CallbackSendsAck)
    {
        // Wire: raw int16 = -18000; ReadFixed16(focPositionScale=100) = -180.0f rad
        EXPECT_CALL(observer, OnSetPositionSetpoint(FloatEq(-180.0f), _)).WillOnce(Invoke([](float, const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, -18000);
        server.HandleMessage(focSetPositionSetpointId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetPositionSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        server.HandleMessage(focSetPositionSetpointId, data);
        EXPECT_EQ(lastAckStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServer, SendCategoryError_EmitsFrameAndAck)
    {
        server.SendCategoryError(focSelectControlModeId, FocMotorCategoryError::modeMismatch);

        EXPECT_EQ(ExtractCanMessageType(lastSentId.Get29BitId()), canCategoryErrorResponseMessageTypeId);
        ASSERT_EQ(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[0], focSelectControlModeId);
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(FocMotorCategoryError::modeMismatch));
        EXPECT_EQ(lastAckStatus, CanAckStatus::categoryError);
        EXPECT_EQ(lastAckCommandType, focSelectControlModeId);
    }

    TEST_F(TestFocMotorCategoryServer, BroadcastFaultStatus_EmitsTelemetryStatusFrame)
    {
        server.BroadcastFaultStatus(FocFaultCode::overCurrent);

        EXPECT_EQ(ExtractCanMessageType(lastSentId.Get29BitId()), focTelemetryStatusResponseId);
        ASSERT_EQ(lastSentData.size(), 6u);
        EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(FocMotorState::fault));
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(FocFaultCode::overCurrent));
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 2), 0);
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 4), 0);
    }
}
