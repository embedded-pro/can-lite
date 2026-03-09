#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::Invoke;
    using testing::NiceMock;
    using testing::StrictMock;

    class FocMotorCategoryServerObserverMock
        : public FocMotorCategoryServerObserver
    {
    public:
        using FocMotorCategoryServerObserver::FocMotorCategoryServerObserver;

        MOCK_METHOD(void, OnQueryMotorType, (), (override));
        MOCK_METHOD(void, OnStart, (), (override));
        MOCK_METHOD(void, OnStop, (), (override));
        MOCK_METHOD(void, OnSetPidCurrent, (const FocPidGains& gains), (override));
        MOCK_METHOD(void, OnSetPidSpeed, (const FocPidGains& gains), (override));
        MOCK_METHOD(void, OnSetPidPosition, (const FocPidGains& gains), (override));
        MOCK_METHOD(void, OnIdentifyElectrical, (), (override));
        MOCK_METHOD(void, OnIdentifyMechanical, (), (override));
        MOCK_METHOD(void, OnRequestTelemetry, (), (override));
        MOCK_METHOD(void, OnSetEncoderResolution, (uint16_t resolution), (override));
    };

    class TestFocMotorCategoryServer : public ::testing::Test
    {
    public:
        TestFocMotorCategoryServer()
        {
            ON_CALL(canMock, SendData(_, _, _))
                .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    }));
        }

        NiceMock<hal::CanMock> canMock;
        CanFrameTransport transport{ canMock, 1 };
        FocMotorCategoryServer server{ transport };
    };

    class TestFocMotorCategoryServerWithObserver : public TestFocMotorCategoryServer
    {
    public:
        StrictMock<FocMotorCategoryServerObserverMock> observer{ server };
    };

    // --- Basic properties ---

    TEST_F(TestFocMotorCategoryServer, Id)
    {
        EXPECT_EQ(server.Id(), focMotorCategoryId);
    }

    TEST_F(TestFocMotorCategoryServer, RequiresSequenceValidation)
    {
        EXPECT_TRUE(server.RequiresSequenceValidation());
    }

    // --- Command handling ---

    TEST_F(TestFocMotorCategoryServerWithObserver, QueryMotorType_NotifiesObserver)
    {
        EXPECT_CALL(observer, OnQueryMotorType());

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focQueryMotorTypeId, data);
    }

    TEST_F(TestFocMotorCategoryServer, QueryMotorType_NoObserverDoesNotCrash)
    {
        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focQueryMotorTypeId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, Start_NotifiesObserver)
    {
        EXPECT_CALL(observer, OnStart());

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focStartId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, Stop_NotifiesObserver)
    {
        EXPECT_CALL(observer, OnStop());

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focStopId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPidCurrent_ParsesGains)
    {
        EXPECT_CALL(observer, OnSetPidCurrent(_)).WillOnce([](const FocPidGains& gains)
            {
                EXPECT_EQ(gains.kp, 100);
                EXPECT_EQ(gains.ki, 200);
                EXPECT_EQ(gains.kd, 300);
            });

        hal::Can::Message data;
        data.resize(7, 0);
        data[0] = 0x01;
        CanFrameCodec::WriteInt16(data, 1, 100);
        CanFrameCodec::WriteInt16(data, 3, 200);
        CanFrameCodec::WriteInt16(data, 5, 300);
        server.HandleMessage(focSetPidCurrentId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidCurrent_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        server.HandleMessage(focSetPidCurrentId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPidSpeed_ParsesGains)
    {
        EXPECT_CALL(observer, OnSetPidSpeed(_)).WillOnce([](const FocPidGains& gains)
            {
                EXPECT_EQ(gains.kp, -500);
                EXPECT_EQ(gains.ki, 1000);
                EXPECT_EQ(gains.kd, 50);
            });

        hal::Can::Message data;
        data.resize(7, 0);
        data[0] = 0x01;
        CanFrameCodec::WriteInt16(data, 1, -500);
        CanFrameCodec::WriteInt16(data, 3, 1000);
        CanFrameCodec::WriteInt16(data, 5, 50);
        server.HandleMessage(focSetPidSpeedId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidSpeed_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        server.HandleMessage(focSetPidSpeedId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetPidPosition_ParsesGains)
    {
        EXPECT_CALL(observer, OnSetPidPosition(_)).WillOnce([](const FocPidGains& gains)
            {
                EXPECT_EQ(gains.kp, 10);
                EXPECT_EQ(gains.ki, 20);
                EXPECT_EQ(gains.kd, 30);
            });

        hal::Can::Message data;
        data.resize(7, 0);
        data[0] = 0x01;
        CanFrameCodec::WriteInt16(data, 1, 10);
        CanFrameCodec::WriteInt16(data, 3, 20);
        CanFrameCodec::WriteInt16(data, 5, 30);
        server.HandleMessage(focSetPidPositionId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetPidPosition_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(6, 0);
        server.HandleMessage(focSetPidPositionId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, IdentifyElectrical_NotifiesObserver)
    {
        EXPECT_CALL(observer, OnIdentifyElectrical());

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focIdentifyElectricalId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, IdentifyMechanical_NotifiesObserver)
    {
        EXPECT_CALL(observer, OnIdentifyMechanical());

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focIdentifyMechanicalId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, RequestTelemetry_NotifiesObserver)
    {
        EXPECT_CALL(observer, OnRequestTelemetry());

        hal::Can::Message data;
        data.push_back(0x01);
        server.HandleMessage(focRequestTelemetryId, data);
    }

    TEST_F(TestFocMotorCategoryServerWithObserver, SetEncoderResolution_ParsesResolution)
    {
        EXPECT_CALL(observer, OnSetEncoderResolution(4096));

        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = 0x01;
        CanFrameCodec::WriteInt16(data, 1, 4096);
        server.HandleMessage(focSetEncoderResolutionId, data);
    }

    TEST_F(TestFocMotorCategoryServer, SetEncoderResolution_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        server.HandleMessage(focSetEncoderResolutionId, data);
    }

    TEST_F(TestFocMotorCategoryServer, UnknownMessageType_ReturnsFalse)
    {
        EXPECT_FALSE(server.HandleMessage(0xFF, hal::Can::Message{}));
    }

    // --- Response sending ---

    TEST_F(TestFocMotorCategoryServer, SendMotorTypeResponse_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::response);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focMotorTypeResponseId);
                ASSERT_EQ(data.size(), 1u);
                EXPECT_EQ(data[0], static_cast<uint8_t>(FocMotorMode::speed));
                cb(true);
            });

        server.SendMotorTypeResponse(FocMotorMode::speed);
    }

    TEST_F(TestFocMotorCategoryServer, SendElectricalParamsResponse_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::response);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focElectricalParamsResponseId);
                ASSERT_EQ(data.size(), 4u);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 0), 1500);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 2), 800);
                cb(true);
            });

        FocElectricalParams params{ 1500, 800 };
        server.SendElectricalParamsResponse(params);
    }

    TEST_F(TestFocMotorCategoryServer, SendMechanicalParamsResponse_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::response);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focMechanicalParamsResponseId);
                ASSERT_EQ(data.size(), 4u);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 0), 5000);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 2), 2000);
                cb(true);
            });

        FocMechanicalParams params{ 5000, 2000 };
        server.SendMechanicalParamsResponse(params);
    }

    TEST_F(TestFocMotorCategoryServer, SendTelemetryElectricalResponse_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::telemetry);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focTelemetryElectricalResponseId);
                ASSERT_EQ(data.size(), 8u);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 0), 240);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 2), 100);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 4), -50);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 6), 25);
                cb(true);
            });

        FocTelemetryElectrical telemetry{ 240, 100, -50, 25 };
        server.SendTelemetryElectricalResponse(telemetry);
    }

    TEST_F(TestFocMotorCategoryServer, SendTelemetryStatusResponse_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::telemetry);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focTelemetryStatusResponseId);
                ASSERT_EQ(data.size(), 6u);
                EXPECT_EQ(data[0], static_cast<uint8_t>(FocMotorState::running));
                EXPECT_EQ(data[1], static_cast<uint8_t>(FocFaultCode::none));
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 2), 3000);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 4), 1800);
                cb(true);
            });

        FocTelemetryStatus status{ FocMotorState::running, FocFaultCode::none, 3000, 1800 };
        server.SendTelemetryStatusResponse(status);
    }
}
