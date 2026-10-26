#include "can-lite/categories/system/CanSystemCategoryClient.hpp"
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

    class SystemCategoryClientTest
        : public testing::Test
    {
    public:
        StrictMock<hal::CanMock> can;
        CanFrameTransport transport{ can, 0x001 };
        CanSequenceSourceStub sequenceSource;
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

    TEST_F(SystemCategoryClientTest, CommandAckMessageTypeIsHandled)
    {
        EXPECT_EQ(client.HandleMessage(canCommandAckMessageTypeId,
                      MakeMessage({ 0x01, 0x02, static_cast<uint8_t>(CanAckStatus::sequenceError) })),
            CanDispatchResult::handled);
    }

    TEST_F(SystemCategoryClientTest, ShortCommandAckDoesNotCrash)
    {
        EXPECT_EQ(client.HandleMessage(canCommandAckMessageTypeId, MakeMessage({ 0x01, 0x02 })), CanDispatchResult::handled);
        EXPECT_EQ(client.HandleMessage(canCommandAckMessageTypeId, hal::Can::Message{}), CanDispatchResult::handled);
    }

    TEST_F(SystemCategoryClientTest, UnknownMessageTypeIsNotHandled)
    {
        EXPECT_EQ(client.HandleMessage(0x7F, hal::Can::Message{}), CanDispatchResult::unknownMessageType);
    }

    TEST_F(SystemCategoryClientTest, CategoryListResponseIsHandled)
    {
        EXPECT_EQ(client.HandleMessage(canCategoryListResponseMessageTypeId, MakeMessage({ 0x00, 0x01, 0x03 })), CanDispatchResult::handled);
    }

    TEST_F(SystemCategoryClientTest, RequestCategoryListSendsAnEmptyCommandWithoutSequence)
    {
        EXPECT_CALL(can, SendData(hal::Can::Id::Create29BitId(MakeCanId(CanPriority::command, canSystemCategoryId, canCategoryListRequestMessageTypeId, 0x02A)), testing::IsEmpty(), testing::_));

        EXPECT_TRUE(client.RequestCategoryList(0x02A));
    }
}
