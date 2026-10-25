#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/core/test/CanSequenceSourceMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace testing;
    using namespace services;

    class CommandingCategoryClient
        : public CanCategoryClient
    {
    public:
        CommandingCategoryClient(CanFrameTransport& transport, CanSequenceSource& sequenceSource)
            : CanCategoryClient(transport, sequenceSource)
        {}

        uint8_t Id() const override
        {
            return 0x03;
        }

        bool Send(const hal::Can::Message& payload, CanPriority priority = CanPriority::command)
        {
            return SendCommand(0x005, 0x10, payload, priority);
        }
    };

    class CanCategoryClientSendCommandTest
        : public Test
    {
    public:
        hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message message;
            for (auto byte : bytes)
                message.push_back(byte);
            return message;
        }

        void OccupyTransportWithOrdinaryFrames()
        {
            EXPECT_CALL(can, SendData(_, _, _));

            for (std::size_t i = 0; i <= CanFrameTransport::queueDepth - CanFrameTransport::emergencyReserve; ++i)
                ASSERT_TRUE(transport.SendFrame(CanPriority::command, 0x03, 0x11, hal::Can::Message{}, [](bool) {}));
        }

        StrictMock<hal::CanMock> can;
        CanFrameTransport transport{ can, 0 };
        StrictMock<CanSequenceSourceMock> sequenceSource;
        CommandingCategoryClient category{ transport, sequenceSource };
    };

    TEST_F(CanCategoryClientSendCommandTest, ReservesTheSequenceBeforeTheFrameReachesTheDriver)
    {
        InSequence order;
        EXPECT_CALL(sequenceSource, ReserveSequence(0x005, 0x03, 0x10)).WillOnce(Return(7));
        EXPECT_CALL(can, SendData(_, ElementsAre(7, 0xaa), _));

        EXPECT_TRUE(category.Send(MakeMessage({ 0xaa })));
    }

    TEST_F(CanCategoryClientSendCommandTest, FrameThatLeavesTheNodeKeepsItsSequence)
    {
        EXPECT_CALL(sequenceSource, ReserveSequence(0x005, 0x03, 0x10)).WillOnce(Return(7));
        EXPECT_CALL(can, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& onDone)
            {
                onDone(true);
            });

        EXPECT_TRUE(category.Send(MakeMessage({ 0xaa })));
    }

    TEST_F(CanCategoryClientSendCommandTest, FrameThatFailsToLeaveTheNodeReturnsItsSequence)
    {
        EXPECT_CALL(sequenceSource, ReserveSequence(0x005, 0x03, 0x10)).WillOnce(Return(7));
        EXPECT_CALL(can, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& onDone)
            {
                onDone(false);
            });
        EXPECT_CALL(sequenceSource, ReturnSequence(0x005, 7));

        EXPECT_TRUE(category.Send(MakeMessage({ 0xaa })));
    }

    TEST_F(CanCategoryClientSendCommandTest, PayloadWithNoRoomForTheSequenceCancelsTheReservation)
    {
        EXPECT_CALL(sequenceSource, ReserveSequence(0x005, 0x03, 0x10)).WillOnce(Return(7));
        EXPECT_CALL(sequenceSource, CancelReservation(0x005, 7));

        EXPECT_FALSE(category.Send(MakeMessage({ 1, 2, 3, 4, 5, 6, 7, 8 })));
    }

    TEST_F(CanCategoryClientSendCommandTest, CommandRefusedByAFullQueueCancelsTheReservation)
    {
        OccupyTransportWithOrdinaryFrames();

        EXPECT_CALL(sequenceSource, ReserveSequence(0x005, 0x03, 0x10)).WillOnce(Return(7));
        EXPECT_CALL(sequenceSource, CancelReservation(0x005, 7));

        EXPECT_FALSE(category.Send(MakeMessage({ 0xaa })));
    }

    TEST_F(CanCategoryClientSendCommandTest, QueuedCommandDisplacedByAnEmergencyFrameReturnsItsSequence)
    {
        EXPECT_CALL(can, SendData(_, _, _));
        for (std::size_t i = 0; i < CanFrameTransport::queueDepth - CanFrameTransport::emergencyReserve; ++i)
            ASSERT_TRUE(transport.SendFrame(CanPriority::command, 0x03, 0x11, hal::Can::Message{}, [](bool) {}));

        EXPECT_CALL(sequenceSource, ReserveSequence(0x005, 0x03, 0x10)).WillOnce(Return(7));
        EXPECT_TRUE(category.Send(MakeMessage({ 0xaa })));

        for (std::size_t i = 0; i < CanFrameTransport::emergencyReserve; ++i)
            ASSERT_TRUE(transport.SendFrame(CanPriority::emergency, 0x03, 0x12, hal::Can::Message{}, [](bool) {}));

        EXPECT_CALL(sequenceSource, ReturnSequence(0x005, 7));
        EXPECT_TRUE(transport.SendFrame(CanPriority::emergency, 0x03, 0x12, hal::Can::Message{}, [](bool) {}));
    }
}
