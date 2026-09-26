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
        static constexpr uint8_t ordinaryCapacity = CanFrameTransport::queueDepth - CanFrameTransport::emergencyReserve;

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

        void CompleteSend(bool success = true)
        {
            auto action = completion;
            completion = nullptr;
            action(success);
        }

        void FillOrdinaryQueue(CanPriority priority, int& failures)
        {
            ExpectOneSend();
            transport.SendFrame(priority, 0x2, 0x00, MakeMessage({}), [](bool) {});

            for (uint8_t i = 0; i != ordinaryCapacity; ++i)
                ASSERT_TRUE(transport.SendFrame(priority, 0x2, static_cast<uint8_t>(0x10 + i), MakeMessage({}), [&failures](bool success)
                    {
                        if (!success)
                            ++failures;
                    }));
        }

        void SendEmergency(uint8_t messageType, int& completions)
        {
            ASSERT_TRUE(transport.SendFrame(CanPriority::emergency, 0x2, messageType, MakeMessage({}), [&completions](bool)
                {
                    ++completions;
                }));
        }

        uint8_t DrainOne()
        {
            ExpectOneSend();
            CompleteSend();
            return ExtractCanMessageType(sentId.Get29BitId());
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

        EXPECT_TRUE(transport.SendFrame(CanPriority::response, 0x2, 0x81, MakeMessage({ 0xAA }), [](bool) {}));

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

        EXPECT_TRUE(transport.SendFrame(0x321, CanPriority::command, 0x3, 0x04, MakeMessage({}), [](bool) {}));

        EXPECT_EQ(ExtractCanNodeId(sentId.Get29BitId()), 0x321);
    }

    TEST_F(CanFrameTransportTest, SetNodeIdAffectsSubsequentFrames)
    {
        transport.SetNodeId(0x777);
        ExpectOneSend();

        transport.SendFrame(CanPriority::heartbeat, 0x0, 0x01, MakeMessage({}), [](bool) {});

        EXPECT_EQ(ExtractCanNodeId(sentId.Get29BitId()), 0x777);
    }

    TEST_F(CanFrameTransportTest, SecondFrameIsQueuedWhileFirstIsInProgress)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({ 0x01 }), [](bool) {});

        EXPECT_TRUE(transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({ 0x02 }), [](bool) {}));
    }

    TEST_F(CanFrameTransportTest, CompletingASendDrainsTheNextQueuedFrame)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({ 0x01 }), [](bool) {});
        transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({ 0x02 }), [](bool) {});

        ExpectOneSend();
        CompleteSend();

        EXPECT_EQ(ExtractCanMessageType(sentId.Get29BitId()), 0x02);
        EXPECT_EQ(sentData, MakeMessage({ 0x02 }));
    }

    TEST_F(CanFrameTransportTest, QueueDrainsInFifoOrder)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [](bool) {});
        transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({}), [](bool) {});
        transport.SendFrame(CanPriority::command, 0x1, 0x03, MakeMessage({}), [](bool) {});

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
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [](bool) {});

        CompleteSend();

        ExpectOneSend();
        EXPECT_TRUE(transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({}), [](bool) {}));
        EXPECT_EQ(ExtractCanMessageType(sentId.Get29BitId()), 0x02);
    }

    TEST_F(CanFrameTransportTest, OnDoneIsInvokedWhenTheFrameCompletes)
    {
        bool done = false;
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [&done](bool)
            {
                done = true;
            });

        EXPECT_FALSE(done);
        CompleteSend();
        EXPECT_TRUE(done);
    }

    TEST_F(CanFrameTransportTest, OnDoneReportsHalSendFailureInsteadOfSwallowingIt)
    {
        bool done = false;
        bool reportedSuccess = true;
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [&done, &reportedSuccess](bool success)
            {
                done = true;
                reportedSuccess = success;
            });

        CompleteSend(false);
        EXPECT_TRUE(done);
        EXPECT_FALSE(reportedSuccess);
    }

    TEST_F(CanFrameTransportTest, QueuedFrameKeepsItsOwnCompletionCallback)
    {
        bool firstDone = false;
        bool secondDone = false;

        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [&firstDone](bool)
            {
                firstDone = true;
            });
        transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({}), [&secondDone](bool)
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

    TEST_F(CanFrameTransportTest, SendIsRejectedOnceTheOrdinaryShareOfTheQueueIsFull)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x00, MakeMessage({}), [](bool) {});

        for (uint8_t i = 0; i != ordinaryCapacity; ++i)
            EXPECT_TRUE(transport.SendFrame(CanPriority::command, 0x1, i, MakeMessage({}), [](bool) {}));

        EXPECT_FALSE(transport.SendFrame(CanPriority::command, 0x1, 0x09, MakeMessage({}), [](bool) {}));
        EXPECT_EQ(transport.Statistics().ordinaryDrops, 1u);
    }

    TEST_F(CanFrameTransportTest, SpaceBecomesAvailableAgainAfterDraining)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x00, MakeMessage({}), [](bool) {});
        for (uint8_t i = 0; i != ordinaryCapacity; ++i)
            transport.SendFrame(CanPriority::command, 0x1, i, MakeMessage({}), [](bool) {});
        ASSERT_FALSE(transport.SendFrame(CanPriority::command, 0x1, 0x09, MakeMessage({}), [](bool) {}));

        ExpectOneSend();
        CompleteSend();

        EXPECT_TRUE(transport.SendFrame(CanPriority::command, 0x1, 0x09, MakeMessage({}), [](bool) {}));
    }

    TEST_F(CanFrameTransportTest, SendNotificationFiresForAnImmediateSend)
    {
        int notifications = 0;
        transport.SetOnSendNotification([&notifications]
            {
                ++notifications;
            });

        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [](bool) {});

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
        transport.SendFrame(CanPriority::command, 0x1, 0x01, MakeMessage({}), [](bool) {});
        transport.SendFrame(CanPriority::command, 0x1, 0x02, MakeMessage({}), [](bool) {});

        EXPECT_EQ(notifications, 2);
    }

    TEST_F(CanFrameTransportTest, SendRawFramePassesTheIdentifierThrough)
    {
        auto id = hal::Can::Id::Create29BitId(0x0ABCDEF);

        ExpectOneSend();
        EXPECT_TRUE(transport.SendRawFrame(id, MakeMessage({ 0x01 }), [](bool) {}));

        EXPECT_EQ(sentId.Get29BitId(), 0x0ABCDEFu);
    }

    TEST_F(CanFrameTransportTest, EmergencyFrameIsAdmittedWhenOrdinaryTrafficHasFilledItsShare)
    {
        int failures = 0;
        int emergencyCompletions = 0;
        FillOrdinaryQueue(CanPriority::telemetry, failures);
        ASSERT_FALSE(transport.SendFrame(CanPriority::telemetry, 0x2, 0x7F, MakeMessage({}), [](bool) {}));

        SendEmergency(0x01, emergencyCompletions);
        SendEmergency(0x02, emergencyCompletions);

        EXPECT_EQ(failures, 0);
        EXPECT_EQ(transport.Statistics().evictions, 0u);
    }

    TEST_F(CanFrameTransportTest, EmergencyFramesAreSentBeforeQueuedLowerPriorityTraffic)
    {
        int failures = 0;
        int emergencyCompletions = 0;
        FillOrdinaryQueue(CanPriority::telemetry, failures);
        SendEmergency(0x01, emergencyCompletions);
        SendEmergency(0x02, emergencyCompletions);

        EXPECT_EQ(DrainOne(), 0x01);
        EXPECT_EQ(DrainOne(), 0x02);
        EXPECT_EQ(DrainOne(), 0x10);
    }

    TEST_F(CanFrameTransportTest, QueuedFramesAreOrderedByPriorityThenArrival)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::command, 0x1, 0x00, MakeMessage({}), [](bool) {});
        transport.SendFrame(CanPriority::telemetry, 0x1, 0x01, MakeMessage({}), [](bool) {});
        transport.SendFrame(CanPriority::response, 0x1, 0x02, MakeMessage({}), [](bool) {});
        transport.SendFrame(CanPriority::telemetry, 0x1, 0x03, MakeMessage({}), [](bool) {});
        transport.SendFrame(CanPriority::heartbeat, 0x1, 0x04, MakeMessage({}), [](bool) {});
        transport.SendFrame(CanPriority::response, 0x1, 0x05, MakeMessage({}), [](bool) {});

        EXPECT_EQ(DrainOne(), 0x02);
        EXPECT_EQ(DrainOne(), 0x05);
        EXPECT_EQ(DrainOne(), 0x01);
        EXPECT_EQ(DrainOne(), 0x03);
        EXPECT_EQ(DrainOne(), 0x04);
    }

    TEST_F(CanFrameTransportTest, EmergencyFrameEvictsTheLowestPriorityNewestFrameWhenTheQueueIsFull)
    {
        int failures = 0;
        int emergencyCompletions = 0;
        int evictedCompletions = 0;
        bool evictedSuccess = true;

        ExpectOneSend();
        transport.SendFrame(CanPriority::response, 0x2, 0x00, MakeMessage({}), [](bool) {});
        for (uint8_t i = 0; i != ordinaryCapacity - 1; ++i)
            transport.SendFrame(CanPriority::response, 0x2, static_cast<uint8_t>(0x10 + i), MakeMessage({}), [&failures](bool success)
                {
                    if (!success)
                        ++failures;
                });
        transport.SendFrame(CanPriority::telemetry, 0x2, 0x30, MakeMessage({}), [&evictedCompletions, &evictedSuccess](bool success)
            {
                ++evictedCompletions;
                evictedSuccess = success;
            });
        SendEmergency(0x01, emergencyCompletions);
        SendEmergency(0x02, emergencyCompletions);

        SendEmergency(0x03, emergencyCompletions);

        EXPECT_EQ(evictedCompletions, 1);
        EXPECT_FALSE(evictedSuccess);
        EXPECT_EQ(failures, 0);
        EXPECT_EQ(transport.Statistics().evictions, 1u);

        for (int i = 0; i != 3 + ordinaryCapacity - 1; ++i)
            EXPECT_NE(DrainOne(), 0x30);
        CompleteSend();

        EXPECT_EQ(evictedCompletions, 1);
        EXPECT_EQ(emergencyCompletions, 3);
    }

    TEST_F(CanFrameTransportTest, EmergencyFrameIsRejectedAndCountedWhenOnlyEmergencyFramesAreQueued)
    {
        int completions = 0;
        ExpectOneSend();
        transport.SendFrame(CanPriority::emergency, 0x2, 0x00, MakeMessage({}), [](bool) {});
        for (uint8_t i = 0; i != CanFrameTransport::queueDepth; ++i)
            SendEmergency(static_cast<uint8_t>(0x10 + i), completions);

        EXPECT_FALSE(transport.SendFrame(CanPriority::emergency, 0x2, 0x7F, MakeMessage({}), [](bool) {}));
        EXPECT_EQ(transport.Statistics().emergencyDrops, 1u);
        EXPECT_EQ(transport.Statistics().evictions, 0u);
    }

    TEST_F(CanFrameTransportTest, OrdinaryFrameIsRefusedWhenEmergencyFramesFillTheQueue)
    {
        int completions = 0;
        ExpectOneSend();
        transport.SendFrame(CanPriority::emergency, 0x2, 0x00, MakeMessage({}), [](bool) {});
        for (uint8_t i = 0; i != CanFrameTransport::queueDepth; ++i)
            SendEmergency(static_cast<uint8_t>(0x10 + i), completions);

        EXPECT_FALSE(transport.SendFrame(CanPriority::telemetry, 0x2, 0x7F, MakeMessage({}), [](bool) {}));
        EXPECT_EQ(transport.Statistics().ordinaryDrops, 1u);
    }

    TEST_F(CanFrameTransportTest, OrdinaryFrameIsRefusedWhenAFullQueueHoldsFewerThanItsShare)
    {
        int completions = 0;
        ExpectOneSend();
        transport.SendFrame(CanPriority::emergency, 0x2, 0x00, MakeMessage({}), [](bool) {});
        ASSERT_TRUE(transport.SendFrame(CanPriority::telemetry, 0x2, 0x40, MakeMessage({}), [](bool) {}));
        for (uint8_t i = 0; i != CanFrameTransport::queueDepth - 1; ++i)
            SendEmergency(static_cast<uint8_t>(0x10 + i), completions);

        EXPECT_FALSE(transport.SendFrame(CanPriority::telemetry, 0x2, 0x7F, MakeMessage({}), [](bool) {}));
        EXPECT_EQ(transport.Statistics().ordinaryDrops, 1u);
    }

    TEST_F(CanFrameTransportTest, FailedEmergencyFrameIsRetriedBeforeQueuedTrafficUpToTheLimit)
    {
        int completions = 0;
        bool reportedSuccess = true;
        ExpectOneSend();
        transport.SendFrame(CanPriority::emergency, 0x2, 0x01, MakeMessage({ 0xEE }), [&completions, &reportedSuccess](bool success)
            {
                ++completions;
                reportedSuccess = success;
            });
        transport.SendFrame(CanPriority::telemetry, 0x2, 0x40, MakeMessage({}), [](bool) {});

        for (uint8_t retry = 0; retry != CanFrameTransport::emergencyRetryLimit; ++retry)
        {
            ExpectOneSend();
            CompleteSend(false);
            EXPECT_EQ(ExtractCanMessageType(sentId.Get29BitId()), 0x01);
            EXPECT_EQ(sentData, MakeMessage({ 0xEE }));
            EXPECT_EQ(completions, 0);
        }

        ExpectOneSend();
        CompleteSend(false);

        EXPECT_EQ(ExtractCanMessageType(sentId.Get29BitId()), 0x40);
        EXPECT_EQ(completions, 1);
        EXPECT_FALSE(reportedSuccess);
        EXPECT_EQ(transport.Statistics().emergencyRetries, CanFrameTransport::emergencyRetryLimit);
        EXPECT_EQ(transport.Statistics().emergencyDrops, 1u);
    }

    TEST_F(CanFrameTransportTest, EmergencyFrameSucceedingOnRetryReportsSuccess)
    {
        bool reportedSuccess = false;
        ExpectOneSend();
        transport.SendFrame(CanPriority::emergency, 0x2, 0x01, MakeMessage({}), [&reportedSuccess](bool success)
            {
                reportedSuccess = success;
            });

        ExpectOneSend();
        CompleteSend(false);
        CompleteSend(true);

        EXPECT_TRUE(reportedSuccess);
        EXPECT_EQ(transport.Statistics().emergencyRetries, 1u);
        EXPECT_EQ(transport.Statistics().emergencyDrops, 0u);
    }

    TEST_F(CanFrameTransportTest, FailedOrdinaryFrameIsNotRetriedAndIsCounted)
    {
        ExpectOneSend();
        transport.SendFrame(CanPriority::telemetry, 0x2, 0x01, MakeMessage({}), [](bool) {});

        CompleteSend(false);

        EXPECT_EQ(transport.Statistics().sendFailures, 1u);
        EXPECT_EQ(transport.Statistics().emergencyRetries, 0u);
    }

    TEST_F(CanFrameTransportTest, RepeatedFaultsUnderContinuousTelemetryAreAllSentAndEveryCallbackRunsOnce)
    {
        int telemetryCompletions = 0;
        int telemetryAccepted = 0;
        int faultCompletions = 0;
        int faultsSent = 0;

        EXPECT_CALL(can, SendData(_, _, _)).WillRepeatedly([this, &faultsSent](hal::Can::Id id, const hal::Can::Message&, const infra::Function<void(bool)>& onDone)
            {
                if (ExtractCanPriority(id.Get29BitId()) == CanPriority::emergency)
                    ++faultsSent;
                completion = onDone;
            });

        for (int cycle = 0; cycle != 200; ++cycle)
        {
            for (int i = 0; i != 3; ++i)
                if (transport.SendFrame(CanPriority::telemetry, 0x2, 0x60, MakeMessage({}), [&telemetryCompletions](bool)
                        {
                            ++telemetryCompletions;
                        }))
                    ++telemetryAccepted;

            if (cycle % 5 == 0)
                SendEmergency(0x01, faultCompletions);

            CompleteSend();
        }

        while (completion)
            CompleteSend();

        EXPECT_EQ(faultCompletions, 40);
        EXPECT_EQ(faultsSent, 40);
        EXPECT_EQ(telemetryCompletions, telemetryAccepted);
        EXPECT_EQ(transport.Statistics().emergencyDrops, 0u);
    }
}
