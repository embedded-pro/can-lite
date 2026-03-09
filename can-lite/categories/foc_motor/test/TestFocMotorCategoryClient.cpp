#include "can-lite/categories/foc_motor/FocMotorCategoryClient.hpp"
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

    class FocMotorCategoryClientObserverMock
        : public FocMotorCategoryClientObserver
    {
    public:
        using FocMotorCategoryClientObserver::FocMotorCategoryClientObserver;

        MOCK_METHOD(void, OnMotorTypeResponse, (FocMotorMode mode), (override));
        MOCK_METHOD(void, OnElectricalParamsResponse, (const FocElectricalParams& params), (override));
        MOCK_METHOD(void, OnMechanicalParamsResponse, (const FocMechanicalParams& params), (override));
        MOCK_METHOD(void, OnTelemetryElectricalResponse, (const FocTelemetryElectrical& telemetry), (override));
        MOCK_METHOD(void, OnTelemetryStatusResponse, (const FocTelemetryStatus& status), (override));
    };

    class TestFocMotorCategoryClient : public ::testing::Test
    {
    public:
        TestFocMotorCategoryClient()
        {
            ON_CALL(canMock, SendData(_, _, _))
                .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    }));
        }

        NiceMock<hal::CanMock> canMock;
        CanFrameTransport transport{ canMock, 1 };
        FocMotorCategoryClient client{ transport };
    };

    class TestFocMotorCategoryClientWithObserver : public TestFocMotorCategoryClient
    {
    public:
        StrictMock<FocMotorCategoryClientObserverMock> observer{ client };
    };

    // --- Basic properties ---

    TEST_F(TestFocMotorCategoryClient, Id)
    {
        EXPECT_EQ(client.Id(), focMotorCategoryId);
    }

    TEST_F(TestFocMotorCategoryClient, RequiresSequenceValidation)
    {
        EXPECT_FALSE(client.RequiresSequenceValidation());
    }

    // --- Response handling ---

    TEST_F(TestFocMotorCategoryClientWithObserver, MotorTypeResponse_ParsesMode)
    {
        EXPECT_CALL(observer, OnMotorTypeResponse(FocMotorMode::speed));

        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(FocMotorMode::speed));
        client.HandleMessage(focMotorTypeResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClient, MotorTypeResponse_EmptyIgnored)
    {
        hal::Can::Message data;
        client.HandleMessage(focMotorTypeResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClient, MotorTypeResponse_NoObserverDoesNotCrash)
    {
        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(FocMotorMode::speed));
        client.HandleMessage(focMotorTypeResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClientWithObserver, ElectricalParamsResponse_ParsesParams)
    {
        EXPECT_CALL(observer, OnElectricalParamsResponse(_)).WillOnce([](const FocElectricalParams& params)
            {
                EXPECT_EQ(params.resistance, 1500);
                EXPECT_EQ(params.inductance, 800);
            });

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt16(data, 0, 1500);
        CanFrameCodec::WriteInt16(data, 2, 800);
        client.HandleMessage(focElectricalParamsResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClient, ElectricalParamsResponse_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        client.HandleMessage(focElectricalParamsResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClientWithObserver, MechanicalParamsResponse_ParsesParams)
    {
        EXPECT_CALL(observer, OnMechanicalParamsResponse(_)).WillOnce([](const FocMechanicalParams& params)
            {
                EXPECT_EQ(params.inertia, 5000);
                EXPECT_EQ(params.friction, 2000);
            });

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt16(data, 0, 5000);
        CanFrameCodec::WriteInt16(data, 2, 2000);
        client.HandleMessage(focMechanicalParamsResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClient, MechanicalParamsResponse_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        client.HandleMessage(focMechanicalParamsResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClientWithObserver, TelemetryElectricalResponse_ParsesTelemetry)
    {
        EXPECT_CALL(observer, OnTelemetryElectricalResponse(_)).WillOnce([](const FocTelemetryElectrical& t)
            {
                EXPECT_EQ(t.voltage, 240);
                EXPECT_EQ(t.maxCurrent, 100);
                EXPECT_EQ(t.iq, -50);
                EXPECT_EQ(t.id, 25);
            });

        hal::Can::Message data;
        data.resize(8, 0);
        CanFrameCodec::WriteInt16(data, 0, 240);
        CanFrameCodec::WriteInt16(data, 2, 100);
        CanFrameCodec::WriteInt16(data, 4, -50);
        CanFrameCodec::WriteInt16(data, 6, 25);
        client.HandleMessage(focTelemetryElectricalResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClient, TelemetryElectricalResponse_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(7, 0);
        client.HandleMessage(focTelemetryElectricalResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClientWithObserver, TelemetryStatusResponse_ParsesStatus)
    {
        EXPECT_CALL(observer, OnTelemetryStatusResponse(_)).WillOnce([](const FocTelemetryStatus& s)
            {
                EXPECT_EQ(s.state, FocMotorState::running);
                EXPECT_EQ(s.fault, FocFaultCode::none);
                EXPECT_EQ(s.speed, 3000);
                EXPECT_EQ(s.position, 1800);
            });

        hal::Can::Message data;
        data.resize(6, 0);
        data[0] = static_cast<uint8_t>(FocMotorState::running);
        data[1] = static_cast<uint8_t>(FocFaultCode::none);
        CanFrameCodec::WriteInt16(data, 2, 3000);
        CanFrameCodec::WriteInt16(data, 4, 1800);
        client.HandleMessage(focTelemetryStatusResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClient, TelemetryStatusResponse_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(5, 0);
        client.HandleMessage(focTelemetryStatusResponseId, data);
    }

    TEST_F(TestFocMotorCategoryClient, UnknownMessageType_ReturnsFalse)
    {
        EXPECT_FALSE(client.HandleMessage(0xFF, hal::Can::Message{}));
    }

    // --- Command sending ---

    TEST_F(TestFocMotorCategoryClient, SendQueryMotorType_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focQueryMotorTypeId);
                ASSERT_EQ(data.size(), 1u);
                EXPECT_EQ(data[0], 0);
                cb(true);
            });

        client.SendQueryMotorType();
    }

    TEST_F(TestFocMotorCategoryClient, SendStart_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focStartId);
                ASSERT_EQ(data.size(), 1u);
                cb(true);
            });

        client.SendStart();
    }

    TEST_F(TestFocMotorCategoryClient, SendStop_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focStopId);
                ASSERT_EQ(data.size(), 1u);
                cb(true);
            });

        client.SendStop();
    }

    TEST_F(TestFocMotorCategoryClient, SendSetPidCurrent_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focSetPidCurrentId);
                ASSERT_EQ(data.size(), 7u);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 1), 100);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 3), 200);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 5), 300);
                cb(true);
            });

        FocPidGains gains{ 100, 200, 300 };
        client.SendSetPidCurrent(gains);
    }

    TEST_F(TestFocMotorCategoryClient, SendSetPidSpeed_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focSetPidSpeedId);
                ASSERT_EQ(data.size(), 7u);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 1), -500);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 3), 1000);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 5), 50);
                cb(true);
            });

        FocPidGains gains{ -500, 1000, 50 };
        client.SendSetPidSpeed(gains);
    }

    TEST_F(TestFocMotorCategoryClient, SendSetPidPosition_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focSetPidPositionId);
                ASSERT_EQ(data.size(), 7u);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 1), 10);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 3), 20);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 5), 30);
                cb(true);
            });

        FocPidGains gains{ 10, 20, 30 };
        client.SendSetPidPosition(gains);
    }

    TEST_F(TestFocMotorCategoryClient, SendIdentifyElectrical_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focIdentifyElectricalId);
                ASSERT_EQ(data.size(), 1u);
                cb(true);
            });

        client.SendIdentifyElectrical();
    }

    TEST_F(TestFocMotorCategoryClient, SendIdentifyMechanical_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focIdentifyMechanicalId);
                ASSERT_EQ(data.size(), 1u);
                cb(true);
            });

        client.SendIdentifyMechanical();
    }

    TEST_F(TestFocMotorCategoryClient, SendRequestTelemetry_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focRequestTelemetryId);
                ASSERT_EQ(data.size(), 1u);
                cb(true);
            });

        client.SendRequestTelemetry();
    }

    TEST_F(TestFocMotorCategoryClient, SendSetEncoderResolution_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& cb)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), focMotorCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), focSetEncoderResolutionId);
                ASSERT_EQ(data.size(), 3u);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 1), 4096);
                cb(true);
            });

        client.SendSetEncoderResolution(4096);
    }

    TEST_F(TestFocMotorCategoryClient, SequenceCounterIncrements)
    {
        uint8_t firstSeq = 0;
        uint8_t secondSeq = 0;

        EXPECT_CALL(canMock, SendData(_, _, _))
            .WillOnce([&firstSeq](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
                {
                    firstSeq = data[0];
                    cb(true);
                })
            .WillOnce([&secondSeq](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
                {
                    secondSeq = data[0];
                    cb(true);
                });

        client.SendStart();
        client.SendStop();

        EXPECT_EQ(firstSeq, 0);
        EXPECT_EQ(secondSeq, 1);
    }
}
