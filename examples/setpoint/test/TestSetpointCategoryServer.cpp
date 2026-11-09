#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanCommandAcknowledgerMock.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "examples/setpoint/SetpointCategoryServer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::ElementsAreArray;
    using testing::FloatEq;
    using testing::StrictMock;

    class SetpointCategoryServerObserverMock
        : public SetpointCategoryServerObserver
    {
    public:
        using SetpointCategoryServerObserver::SetpointCategoryServerObserver;

        MOCK_METHOD(void, OnSetSetpoint, (float setpoint, const infra::Function<void(CanAckStatus)>& onDone), (override));
        MOCK_METHOD(void, OnRequestStatus, (const infra::Function<void(SetpointStatus)>& onResult), (override));
    };

    class TestSetpointCategoryServer : public ::testing::Test
    {
    public:
        TestSetpointCategoryServer()
        {
            server.SetAcknowledger(acknowledger);
        }

        void ExpectFrame(CanPriority priority, uint8_t messageType, std::initializer_list<uint8_t> bytes)
        {
            EXPECT_CALL(canMock, SendData(hal::Can::Id::Create29BitId(MakeCanId(priority, setpointCategoryId, messageType, 1)), ElementsAreArray(bytes), _))
                .WillOnce([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& onSent)
                    {
                        onSent(true);
                    });
        }

        void ExpectAck(uint8_t commandId, CanAckStatus status)
        {
            EXPECT_CALL(acknowledger, SendCommandAck(setpointCategoryId, commandId, status));
        }

        static hal::Can::Message SetCommand(int16_t fixedSetpoint)
        {
            hal::Can::Message data;
            data.resize(3, 0);
            CanFrameCodec::WriteInt16(data, 1, fixedSetpoint);
            return data;
        }

        StrictMock<hal::CanMock> canMock;
        CanFrameTransport transport{ canMock, 1 };
        SetpointCategoryServer server{ transport };
        StrictMock<CanCommandAcknowledgerMock> acknowledger;
    };

    class TestSetpointCategoryServerWithObserver : public TestSetpointCategoryServer
    {
    public:
        StrictMock<SetpointCategoryServerObserverMock> observer{ server };
    };

    TEST_F(TestSetpointCategoryServer, Id)
    {
        EXPECT_EQ(server.Id(), setpointCategoryId);
    }

    TEST_F(TestSetpointCategoryServer, RequiresSequenceValidation)
    {
        EXPECT_TRUE(server.RequiresSequenceValidation());
    }

    TEST_F(TestSetpointCategoryServerWithObserver, SetSetpoint_DecodesValueAndAcksSuccess)
    {
        EXPECT_CALL(observer, OnSetSetpoint(FloatEq(21.5f), _)).WillOnce([](float, const infra::Function<void(CanAckStatus)>& onDone)
            {
                onDone(CanAckStatus::success);
            });
        ExpectAck(setpointSetId, CanAckStatus::success);

        EXPECT_EQ(server.HandleMessage(setpointSetId, SetCommand(2150)), CanDispatchResult::handled);
    }

    TEST_F(TestSetpointCategoryServerWithObserver, SetSetpoint_AcksWithTheStatusTheObserverReports)
    {
        EXPECT_CALL(observer, OnSetSetpoint(FloatEq(-3.0f), _)).WillOnce([](float, const infra::Function<void(CanAckStatus)>& onDone)
            {
                onDone(CanAckStatus::invalidState);
            });
        ExpectAck(setpointSetId, CanAckStatus::invalidState);

        server.HandleMessage(setpointSetId, SetCommand(-300));
    }

    TEST_F(TestSetpointCategoryServerWithObserver, SetSetpoint_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(2, 0);

        EXPECT_EQ(server.HandleMessage(setpointSetId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestSetpointCategoryServerWithObserver, RequestStatus_SendsStatusResponseAndAck)
    {
        EXPECT_CALL(observer, OnRequestStatus(_)).WillOnce([](const infra::Function<void(SetpointStatus)>& onResult)
            {
                onResult(SetpointStatus{ 21.5f, 20.25f });
            });
        ExpectFrame(CanPriority::response, setpointStatusId, { 0x08, 0x66, 0x07, 0xE9 });
        ExpectAck(setpointRequestStatusId, CanAckStatus::success);

        hal::Can::Message data;
        data.push_back(0x01);
        EXPECT_EQ(server.HandleMessage(setpointRequestStatusId, data), CanDispatchResult::handled);
    }

    TEST_F(TestSetpointCategoryServer, PublishStatus_SendsStatusAsTelemetry)
    {
        ExpectFrame(CanPriority::telemetry, setpointStatusId, { 0x08, 0x66, 0x07, 0xE9 });

        server.PublishStatus(SetpointStatus{ 21.5f, 20.25f });
    }

    TEST_F(TestSetpointCategoryServer, UnknownMessageType_ReturnsUnknownMessageType)
    {
        EXPECT_EQ(server.HandleMessage(0x7F, hal::Can::Message{}), CanDispatchResult::unknownMessageType);
    }
}
