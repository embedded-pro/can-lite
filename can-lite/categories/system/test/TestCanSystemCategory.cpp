#include "can-lite/categories/system/CanSystemCategoryClient.hpp"
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

    class SystemClientObserverMock
        : public CanSystemCategoryClientObserver
    {
    public:
        using CanSystemCategoryClientObserver::CanSystemCategoryClientObserver;

        MOCK_METHOD(void, OnCategoryListResponse, (const hal::Can::Message& categoryIds), (override));
    };

    class SystemCategoryTest
        : public testing::Test
    {
    public:
        StrictMock<hal::CanMock> can;
        CanFrameTransport transport{ can, 0x001 };
        CanSequenceSourceStub sequenceSource;
    };

    // --- Server ---

    class SystemCategoryServerTest
        : public SystemCategoryTest
    {
    public:
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
        EXPECT_FALSE(server.HandleMessage(0x7F, hal::Can::Message{}));
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

        EXPECT_TRUE(server.HandleMessage(canHeartbeatMessageTypeId, MakeMessage({ canProtocolVersion })));
    }

    TEST_F(SystemCategoryServerWithObserverTest, HeartbeatWithoutPayloadReportsVersionZero)
    {
        EXPECT_CALL(observer, OnHeartbeatReceived(0));

        EXPECT_TRUE(server.HandleMessage(canHeartbeatMessageTypeId, hal::Can::Message{}));
    }

    TEST_F(SystemCategoryServerWithObserverTest, StatusRequestNotifiesObserver)
    {
        EXPECT_CALL(observer, OnStatusRequest());

        EXPECT_TRUE(server.HandleMessage(canStatusRequestMessageTypeId, hal::Can::Message{}));
    }

    TEST_F(SystemCategoryServerWithObserverTest, CategoryListRequestNotifiesObserver)
    {
        EXPECT_CALL(observer, OnCategoryListRequest());

        EXPECT_TRUE(server.HandleMessage(canCategoryListRequestMessageTypeId, hal::Can::Message{}));
    }

    // --- Client ---

    class SystemCategoryClientTest
        : public SystemCategoryTest
    {
    public:
        CanSystemCategoryClient client{ transport, sequenceSource };
    };

    TEST_F(SystemCategoryClientTest, ReportsSystemCategoryId)
    {
        EXPECT_EQ(client.Id(), canSystemCategoryId);
    }

    TEST_F(SystemCategoryClientTest, DoesNotRequireSequenceValidation)
    {
        EXPECT_FALSE(client.RequiresSequenceValidation());
    }

    TEST_F(SystemCategoryClientTest, CommandAckInvokesTheAckCallback)
    {
        struct Ack
        {
            uint8_t category{};
            uint8_t command{};
            CanAckStatus status{ CanAckStatus::success };
            int calls{};
        } ack;

        client.onCommandAck = [&ack](uint8_t category, uint8_t command, CanAckStatus status)
        {
            ack.category = category;
            ack.command = command;
            ack.status = status;
            ++ack.calls;
        };

        EXPECT_TRUE(client.HandleMessage(canCommandAckMessageTypeId,
            MakeMessage({ 0x01, 0x02, static_cast<uint8_t>(CanAckStatus::sequenceError) })));

        EXPECT_EQ(ack.calls, 1);
        EXPECT_EQ(ack.category, 0x01);
        EXPECT_EQ(ack.command, 0x02);
        EXPECT_EQ(ack.status, CanAckStatus::sequenceError);
    }

    TEST_F(SystemCategoryClientTest, CommandAckIsIgnoredWhenNoCallbackIsSet)
    {
        EXPECT_TRUE(client.HandleMessage(canCommandAckMessageTypeId,
            MakeMessage({ 0x01, 0x02, static_cast<uint8_t>(CanAckStatus::success) })));
    }

    TEST_F(SystemCategoryClientTest, ShortCommandAckIsRejected)
    {
        int calls = 0;
        client.onCommandAck = [&calls](uint8_t, uint8_t, CanAckStatus)
        {
            ++calls;
        };

        EXPECT_TRUE(client.HandleMessage(canCommandAckMessageTypeId, MakeMessage({ 0x01, 0x02 })));

        EXPECT_EQ(calls, 0);
    }

    TEST_F(SystemCategoryClientTest, UnknownMessageTypeIsNotHandled)
    {
        EXPECT_FALSE(client.HandleMessage(0x7F, hal::Can::Message{}));
    }

    class SystemCategoryClientWithObserverTest
        : public SystemCategoryClientTest
    {
    public:
        StrictMock<SystemClientObserverMock> observer{ client };
    };

    TEST_F(SystemCategoryClientWithObserverTest, CategoryListResponseIsForwardedToObserver)
    {
        auto categoryIds = MakeMessage({ 0x00, 0x01, 0x03 });

        EXPECT_CALL(observer, OnCategoryListResponse(categoryIds));

        EXPECT_TRUE(client.HandleMessage(canCategoryListResponseMessageTypeId, categoryIds));
    }

    TEST_F(SystemCategoryClientWithObserverTest, EmptyCategoryListResponseIsStillForwarded)
    {
        hal::Can::Message empty;

        EXPECT_CALL(observer, OnCategoryListResponse(empty));

        EXPECT_TRUE(client.HandleMessage(canCategoryListResponseMessageTypeId, empty));
    }
}
