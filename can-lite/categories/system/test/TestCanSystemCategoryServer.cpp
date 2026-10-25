#include "can-lite/categories/system/CanSystemCategoryServer.hpp"
#include "can-lite/core/test/CanCategoryStubs.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::StrictMock;

    hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
    {
        hal::Can::Message message;
        for (auto byte : bytes)
            message.push_back(byte);
        return message;
    }

    class SystemServerObserverMock
        : public CanSystemCategoryServerObserver
    {
    public:
        using CanSystemCategoryServerObserver::CanSystemCategoryServerObserver;

        MOCK_METHOD(void, OnHeartbeatReceived, (uint8_t version), (override));
        MOCK_METHOD(void, OnStatusRequest, (), (override));
        MOCK_METHOD(void, OnCategoryListRequest, (), (override));
    };

    class SystemCategoryServerTest
        : public testing::Test
    {
    public:
        StrictMock<hal::CanMock> can;
        CanFrameTransport transport{ can, 0x001 };
        CanSystemCategoryServer server{ transport };
    };

    TEST_F(SystemCategoryServerTest, ReportsSystemCategoryId)
    {
        EXPECT_EQ(server.Id(), canSystemCategoryId);
    }

    TEST_F(SystemCategoryServerTest, DoesNotRequireSequenceValidation)
    {
        EXPECT_FALSE(server.RequiresSequenceValidation());
    }

    TEST_F(SystemCategoryServerTest, UnknownMessageTypeIsNotHandled)
    {
        EXPECT_EQ(server.HandleMessage(0x7F, hal::Can::Message{}), CanDispatchResult::unknownMessageType);
    }

    class SystemCategoryServerWithObserverTest
        : public SystemCategoryServerTest
    {
    public:
        StrictMock<SystemServerObserverMock> observer{ server };
    };

    TEST_F(SystemCategoryServerWithObserverTest, HeartbeatReportsProtocolVersion)
    {
        EXPECT_CALL(observer, OnHeartbeatReceived(canProtocolVersion));

        EXPECT_EQ(server.HandleMessage(canHeartbeatMessageTypeId, MakeMessage({ canProtocolVersion })), CanDispatchResult::handled);
    }

    TEST_F(SystemCategoryServerWithObserverTest, HeartbeatWithoutPayloadReportsVersionZero)
    {
        EXPECT_CALL(observer, OnHeartbeatReceived(0));

        EXPECT_EQ(server.HandleMessage(canHeartbeatMessageTypeId, hal::Can::Message{}), CanDispatchResult::handled);
    }

    TEST_F(SystemCategoryServerWithObserverTest, StatusRequestNotifiesObserver)
    {
        EXPECT_CALL(observer, OnStatusRequest());

        EXPECT_EQ(server.HandleMessage(canStatusRequestMessageTypeId, hal::Can::Message{}), CanDispatchResult::handled);
    }

    TEST_F(SystemCategoryServerWithObserverTest, CategoryListRequestNotifiesObserver)
    {
        EXPECT_CALL(observer, OnCategoryListRequest());

        EXPECT_EQ(server.HandleMessage(canCategoryListRequestMessageTypeId, hal::Can::Message{}), CanDispatchResult::handled);
    }
}
