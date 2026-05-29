#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::AnyNumber;
    using testing::Invoke;
    using testing::StrictMock;

    class FocMotorCategoryServerObserverMock
        : public FocMotorCategoryServerObserver
    {
    public:
        using FocMotorCategoryServerObserver::FocMotorCategoryServerObserver;

        MOCK_METHOD(void, OnQueryMotorType, (const infra::Function<void(FocMotorMode)>& onResult), (override));
        MOCK_METHOD(void, OnStart, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnStop, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPidCurrent, (const FocPidGains& gains, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPidSpeed, (const FocPidGains& gains, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPidPosition, (const FocPidGains& gains, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnIdentifyElectrical, (const infra::Function<void(FocElectricalParams)>& onResult), (override));
        MOCK_METHOD(void, OnIdentifyMechanical, (const infra::Function<void(FocMechanicalParams)>& onResult), (override));
        MOCK_METHOD(void, OnRequestTelemetry, (const infra::Function<void(FocTelemetryElectrical, FocTelemetryStatus)>& onResult), (override));
        MOCK_METHOD(void, OnSetEncoderResolution, (uint16_t resolution, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSelectControlMode, (FocMotorMode mode, const infra::Function<void(FocMotorMode)>& onActivated), (override));
        MOCK_METHOD(void, OnSetTorqueSetpoint, (int16_t value, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetSpeedSetpoint, (int16_t value, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnSetPositionSetpoint, (int16_t value, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnClearFault, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnEmergencyStop, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnConfigureTelemetryRate, (uint8_t rateHz, const infra::Function<void()>& onDone), (override));
    };

    class AcknowledgerSpy
        : public CanCommandAcknowledger
    {
    public:
        void SendCommandAck(uint8_t categoryId, uint8_t commandType, CanAckStatus status) override
        {
            lastCategoryId = categoryId;
            lastCommandType = commandType;
            lastStatus = status;
            ackCount++;
        }

        uint8_t lastCategoryId{ 0 };
        uint8_t lastCommandType{ 0 };
        CanAckStatus lastStatus{ CanAckStatus::success };
        std::size_t ackCount{ 0 };
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
            server.SetAcknowledger(acknowledger);
        }

        StrictMock<hal::CanMock> canMock;
        CanFrameTransport transport{ canMock, 1 };
        FocMotorCategoryServer server{ transport };
        AcknowledgerSpy acknowledger;
        hal::Can::Id lastSentId{ hal::Can::Id::Create29BitId(0) };
        hal::Can::Message lastSentData;
        std::size_t sendCount{ 0 };
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
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
        EXPECT_EQ(acknowledger.lastCommandType, focQueryMotorTypeId);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, Start_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnStart(_)).WillOnce(Invoke([](const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focStartId, data);

        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
        EXPECT_EQ(acknowledger.lastCommandType, focStartId);
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

        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
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
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidCurrent_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        server.HandleMessage(focSetPidCurrentId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
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
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
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
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, IdentifyElectrical_CallbackSendsResponse)
    {
        EXPECT_CALL(observer, OnIdentifyElectrical(_)).WillOnce(Invoke([](const infra::Function<void(FocElectricalParams)>& cb)
            {
                cb(FocElectricalParams{ 1500, 800 });
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focIdentifyElectricalId, data);

        EXPECT_EQ(ExtractCanMessageType(lastSentId.Get29BitId()), focElectricalParamsResponseId);
        ASSERT_EQ(lastSentData.size(), 4u);
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 0), 1500);
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 2), 800);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, IdentifyMechanical_CallbackSendsResponse)
    {
        EXPECT_CALL(observer, OnIdentifyMechanical(_)).WillOnce(Invoke([](const infra::Function<void(FocMechanicalParams)>& cb)
            {
                cb(FocMechanicalParams{ 5000, 2000 });
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focIdentifyMechanicalId, data);

        EXPECT_EQ(ExtractCanMessageType(lastSentId.Get29BitId()), focMechanicalParamsResponseId);
        ASSERT_EQ(lastSentData.size(), 4u);
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 0), 5000);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, RequestTelemetry_CallbackSendsBothFrames)
    {
        EXPECT_CALL(observer, OnRequestTelemetry(_)).WillOnce(Invoke([](const infra::Function<void(FocTelemetryElectrical, FocTelemetryStatus)>& cb)
            {
                cb(FocTelemetryElectrical{ 240, 100, -50, 25 }, FocTelemetryStatus{ FocMotorState::running, FocFaultCode::none, 3000, 1800 });
            }));

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focRequestTelemetryId, data);

        EXPECT_EQ(sendCount, 2u);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
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
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetEncoderResolution_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        server.HandleMessage(focSetEncoderResolutionId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
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
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetTorqueSetpoint_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnSetTorqueSetpoint(500, _)).WillOnce(Invoke([](int16_t, const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 500);
        server.HandleMessage(focSetTorqueSetpointId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetTorqueSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        server.HandleMessage(focSetTorqueSetpointId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServer, SelectControlMode_InvalidModeRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = 0xFF;
        server.HandleMessage(focSelectControlModeId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
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
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
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
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
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
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, ConfigureTelemetryRate_TooShortRejected)
    {
        hal::Can::Message data;
        data.push_back(0);
        server.HandleMessage(focConfigureTelemetryRateId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServer, SelectControlMode_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(1, 0);
        server.HandleMessage(focSelectControlModeId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetSpeedSetpoint_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnSetSpeedSetpoint(1500, _)).WillOnce(Invoke([](int16_t, const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, 1500);
        server.HandleMessage(focSetSpeedSetpointId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetSpeedSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        server.HandleMessage(focSetSpeedSetpointId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPositionSetpoint_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnSetPositionSetpoint(-18000, _)).WillOnce(Invoke([](int16_t, const infra::Function<void()>& cb)
            {
                cb();
            }));

        hal::Can::Message data;
        data.resize(3, 0);
        CanFrameCodec::WriteInt16(data, 1, -18000);
        server.HandleMessage(focSetPositionSetpointId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFocMotorCategoryServer, SetPositionSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        server.HandleMessage(focSetPositionSetpointId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFocMotorCategoryServer, SendCategoryError_EmitsFrameAndAck)
    {
        server.SendCategoryError(focSelectControlModeId, FocMotorCategoryError::modeMismatch);

        EXPECT_EQ(ExtractCanMessageType(lastSentId.Get29BitId()), focCategoryErrorResponseId);
        ASSERT_EQ(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[0], focSelectControlModeId);
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(FocMotorCategoryError::modeMismatch));
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::categoryError);
        EXPECT_EQ(acknowledger.lastCommandType, focSelectControlModeId);
    }
}
