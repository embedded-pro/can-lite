#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "examples/setpoint/SetpointCategoryClient.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::StrictMock;

    class SetpointCategoryClientObserverMock
        : public SetpointCategoryClientObserver
    {
    public:
        using SetpointCategoryClientObserver::SetpointCategoryClientObserver;

        MOCK_METHOD(void, OnStatus, (const SetpointStatus& status), (override));
    };

    class TestSetpointCategoryClient
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        static hal::Can::Message StatusFrame(int16_t fixedSetpoint, int16_t fixedMeasurement)
        {
            hal::Can::Message data;
            data.resize(4, 0);
            CanFrameCodec::WriteInt16(data, 0, fixedSetpoint);
            CanFrameCodec::WriteInt16(data, 2, fixedMeasurement);
            return data;
        }

        StrictMock<hal::CanMock> canMock;

        struct FixtureInit
        {
            explicit FixtureInit(StrictMock<hal::CanMock>& canMock)
            {
                EXPECT_CALL(canMock, ReceiveData(_)).Times(2);
            }
        } fixtureInit{ canMock };

        CanProtocolClient protocolClient{ canMock };
        CanFrameTransport transport{ canMock, 1 };
        SetpointCategoryClient client{ transport, protocolClient };
    };

    class TestSetpointCategoryClientWithObserver : public TestSetpointCategoryClient
    {
    public:
        StrictMock<SetpointCategoryClientObserverMock> observer{ client };
    };

    TEST_F(TestSetpointCategoryClient, Id)
    {
        EXPECT_EQ(client.Id(), setpointCategoryId);
    }

    TEST_F(TestSetpointCategoryClient, RequiresSequenceValidation)
    {
        EXPECT_FALSE(client.RequiresSequenceValidation());
    }

    TEST_F(TestSetpointCategoryClient, SendSetSetpoint_SendsSequencedCommandWithFixedPointValue)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& onSent)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), setpointCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), setpointSetId);
                EXPECT_EQ(ExtractCanNodeId(rawId), 5);
                ASSERT_EQ(data.size(), 3u);
                EXPECT_EQ(data[0], 0);
                EXPECT_EQ(CanFrameCodec::ReadInt16(data, 1), 2150);
                onSent(true);
            });

        EXPECT_TRUE(client.SendSetSetpoint(5, 21.5f));
    }

    TEST_F(TestSetpointCategoryClient, SendRequestStatus_SendsSequencedCommandWithoutPayload)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto& onSent)
            {
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
                EXPECT_EQ(ExtractCanCategory(rawId), setpointCategoryId);
                EXPECT_EQ(ExtractCanMessageType(rawId), setpointRequestStatusId);
                ASSERT_EQ(data.size(), 1u);
                EXPECT_EQ(data[0], 0);
                onSent(true);
            });

        EXPECT_TRUE(client.SendRequestStatus(5));
    }

    TEST_F(TestSetpointCategoryClientWithObserver, Status_DecodesAndNotifiesObserver)
    {
        EXPECT_CALL(observer, OnStatus(_)).WillOnce([](const SetpointStatus& status)
            {
                EXPECT_FLOAT_EQ(status.setpoint, 21.5f);
                EXPECT_FLOAT_EQ(status.measurement, 20.25f);
            });

        EXPECT_EQ(client.HandleMessage(setpointStatusId, StatusFrame(2150, 2025)), CanDispatchResult::handled);
    }

    TEST_F(TestSetpointCategoryClientWithObserver, Status_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(3, 0);

        EXPECT_EQ(client.HandleMessage(setpointStatusId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestSetpointCategoryClient, Status_WithoutObserverIsHandled)
    {
        EXPECT_EQ(client.HandleMessage(setpointStatusId, StatusFrame(2150, 2025)), CanDispatchResult::handled);
    }
}
