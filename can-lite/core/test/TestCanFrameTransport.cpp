#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::DoAll;
    using testing::SaveArg;
    using testing::StrictMock;

    hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
    {
        hal::Can::Message message;
        for (auto byte : bytes)
            message.push_back(byte);
        return message;
    }

    class CanFrameTransportTest
        : public testing::Test
    {
    public:
        static constexpr uint16_t ownNodeId = 0x123;

        StrictMock<hal::CanMock> can;
        CanFrameTransport transport{ can, ownNodeId };

        infra::Function<void(bool)> completion;
        hal::Can::Id sentId{ hal::Can::Id::Create29BitId(0) };
        hal::Can::Message sentData;

        void ExpectOneSend()
        {
            EXPECT_CALL(can, SendData(_, _, _))
                .WillOnce(DoAll(SaveArg<0>(&sentId), SaveArg<1>(&sentData), SaveArg<2>(&completion)));
        }

        void CompleteSend()
        {
            auto action = completion;
            completion = nullptr;
            action(true);
        }
    };

    TEST_F(CanFrameTransportTest, NodeIdIsReportedAndUpdatable)
    {
        EXPECT_EQ(transport.NodeId(), ownNodeId);

        transport.SetNodeId(0x456);

        EXPECT_EQ(transport.NodeId(), 0x456);
    }

    TEST_F(CanFrameTransportTest, SendFrameUsesOwnNodeIdAsTarget)
    {
        ExpectOneSend();

        EXPECT_TRUE(transport.SendFrame(CanPriority::response, 0x2, 0x81, MakeMessage({ 0xAA }), [] {}));

        auto rawId = sentId.Get29BitId();
        EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::response);
        EXPECT_EQ(ExtractCanCategory(rawId), 0x2);
        EXPECT_EQ(ExtractCanMessageType(rawId), 0x81);
        EXPECT_EQ(ExtractCanNodeId(rawId), ownNodeId);
        EXPECT_EQ(sentData, MakeMessage({ 0xAA }));
    }

    TEST_F(CanFrameTransportTest, SendFrameHonoursExplicitTargetNode)
    {
        ExpectOneSend();

        EXPECT_TRUE(transport.SendFrame(0x321, CanPriority::command, 0x3, 0x04, MakeMessage({}), [] {}));

        EXPECT_EQ(ExtractCanNodeId(sentId.Get29BitId()), 0x321);
    }

    TEST_F(CanFrameTransportTest, SetNodeIdAffectsSubsequentFrames)
    {
        transport.SetNodeId(0x777);
        ExpectOneSend();

        transport.SendFrame(CanPriority::heartbeat, 0x0, 0x01, MakeMessage({}), [] {});

        EXPECT_EQ(ExtractCanNodeId(sentId.Get29BitId()), 0x777);
    }

    TEST_F(CanFrameTransportTest, SecondFrameIsQueuedWhileFirstIsInProgress)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({ 0x01 }), [] {});

        EXPECT_TRUE(transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({ 0x02 }), [] {}));
    }

    TEST_F(CanFrameTransportTest, CompletingASendDrainsTheNextQueuedFrame)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({ 0x01 }), [] {});
        transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({ 0x02 }), [] {});

        ExpectOneSend();
        CompleteSend();

        EXPECT_EQ(ExtractCanMessageType(sentId.Get29BitId()), 0x02);
        EXPECT_EQ(sentData, MakeMessage({ 0x02 }));
    }

    TEST_F(CanFrameTransportTest, QueueDrainsInFifoOrder)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [] {});
        transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({}), [] {});
        transport.SendFrame(CanPriority::command, 0x1, 0x03, MakeMessage({}), [] {});

        ExpectOneSend();
        CompleteSend();
        EXPECT_EQ(ExtractCanMessageType(sentId.Get29BitId()), 0x02);

        ExpectOneSend();
        CompleteSend();
        EXPECT_EQ(ExtractCanMessageType(sentId.Get29BitId()), 0x03);
    }

    TEST_F(CanFrameTransportTest, DrainingAnEmptyQueueReleasesTheTransport)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [] {});

        CompleteSend();

        ExpectOneSend();
        EXPECT_TRUE(transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({}), [] {}));
        EXPECT_EQ(ExtractCanMessageType(sentId.Get29BitId()), 0x02);
    }

    TEST_F(CanFrameTransportTest, OnDoneIsInvokedWhenTheFrameCompletes)
    {
        bool done = false;
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [&done]
            {
                done = true;
            });

        EXPECT_FALSE(done);
        CompleteSend();
        EXPECT_TRUE(done);
    }

    TEST_F(CanFrameTransportTest, QueuedFrameKeepsItsOwnCompletionCallback)
    {
        bool firstDone = false;
        bool secondDone = false;

        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [&firstDone]
            {
                firstDone = true;
            });
        transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({}), [&secondDone]
            {
                secondDone = true;
            });

        ExpectOneSend();
        CompleteSend();
        EXPECT_TRUE(firstDone);
        EXPECT_FALSE(secondDone);

        CompleteSend();
        EXPECT_TRUE(secondDone);
    }

    TEST_F(CanFrameTransportTest, SendIsRejectedOnceTheQueueIsFull)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x00, MakeMessage({}), [] {});

        for (uint8_t i = 0; i != 8; ++i)
            EXPECT_TRUE(transport.SendFrame(CanPriority::command, 0x1, i, MakeMessage({}), [] {}));

        EXPECT_FALSE(transport.SendFrame(CanPriority::command, 0x1, 0x09, MakeMessage({}), [] {}));
    }

    TEST_F(CanFrameTransportTest, SpaceBecomesAvailableAgainAfterDraining)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x00, MakeMessage({}), [] {});
        for (uint8_t i = 0; i != 8; ++i)
            transport.SendFrame(CanPriority::command, 0x1, i, MakeMessage({}), [] {});
        ASSERT_FALSE(transport.SendFrame(CanPriority::command, 0x1, 0x09, MakeMessage({}), [] {}));

        ExpectOneSend();
        CompleteSend();

        EXPECT_TRUE(transport.SendFrame(CanPriority::command, 0x1, 0x09, MakeMessage({}), [] {}));
    }

    TEST_F(CanFrameTransportTest, SendNotificationFiresForAnImmediateSend)
    {
        int notifications = 0;
        transport.SetOnSendNotification([&notifications]
            {
                ++notifications;
            });

        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [] {});

        EXPECT_EQ(notifications, 1);
    }

    TEST_F(CanFrameTransportTest, SendNotificationFiresForAQueuedSend)
    {
        int notifications = 0;
        transport.SetOnSendNotification([&notifications]
            {
                ++notifications;
            });

        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [] {});
        transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({}), [] {});

        EXPECT_EQ(notifications, 2);
    }

    TEST_F(CanFrameTransportTest, SendRawFramePassesTheIdentifierThrough)
    {
        auto id = hal::Can::Id::Create29BitId(0x0ABCDEF);

        ExpectOneSend();
        EXPECT_TRUE(transport.SendRawFrame(id, MakeMessage({ 0x01 }), [] {}));

        EXPECT_EQ(sentId.Get29BitId(), 0x0ABCDEFu);
    }
}
