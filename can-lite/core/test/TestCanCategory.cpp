#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageType.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanCategoryStubs.hpp"
#include "can-lite/core/test/CanCommandAcknowledgerMock.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/core/test/CanSequenceSourceMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace testing;
    using namespace services;

    class StubMessageType : public CanMessageType
    {
    public:
        explicit StubMessageType(uint8_t id)
            : id(id)
        {}

        uint8_t Id() const override
        {
            return id;
        }

        bool Handle(const hal::Can::Message& data) override
        {
            lastDataSize = data.size();
            handleCallCount++;
            return true;
        }

        std::size_t lastDataSize = 0;
        int handleCallCount = 0;

    private:
        uint8_t id;
    };

    class StubCategoryServer
        : public CanCategoryServerStub
    {
    public:
        explicit StubCategoryServer(uint8_t id)
            : id(id)
        {}

        uint8_t Id() const override
        {
            return id;
        }

    private:
        uint8_t id;
    };

    class StubCategoryClient
        : public CanCategoryClientStub
    {
    public:
        explicit StubCategoryClient(uint8_t id)
            : id(id)
        {}

        uint8_t Id() const override
        {
            return id;
        }

    private:
        uint8_t id;
    };

    class FaultReportingCategoryServer
        : public CanCategoryServer
    {
    public:
        explicit FaultReportingCategoryServer(CanFrameTransport& transport)
            : CanCategoryServer(transport)
        {}

        uint8_t Id() const override
        {
            return 0x03;
        }

        bool ReportFault(uint8_t code)
        {
            CanPayloadWriter payload;
            payload.WriteUInt8(code);
            return SendEmergency(0x90, payload);
        }
    };

    class StubPduMessageType : public CanMessageType
    {
    public:
        explicit StubPduMessageType(uint8_t id)
            : id(id)
        {}

        uint8_t Id() const override
        {
            return id;
        }

        bool Handle(const hal::Can::Message&) override
        {
            return true;
        }

        bool HandlePdu(infra::ConstByteRange data) override
        {
            lastPduSize = data.size();
            handlePduCallCount++;
            return true;
        }

        std::size_t lastPduSize = 0;
        int handlePduCallCount = 0;

    private:
        uint8_t id;
    };

    TEST(CanCategoryTest, SendEmergencyUsesEmergencyPriority)
    {
        testing::StrictMock<hal::CanMock> can;
        CanFrameTransport transport{ can, 0x042 };
        FaultReportingCategoryServer category{ transport };

        hal::Can::Id sentId{ hal::Can::Id::Create29BitId(0) };
        hal::Can::Message sentData;
        EXPECT_CALL(can, SendData(testing::_, testing::_, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<0>(&sentId), testing::SaveArg<1>(&sentData)));

        EXPECT_TRUE(category.ReportFault(0x07));

        EXPECT_EQ(ExtractCanPriority(sentId.Get29BitId()), CanPriority::emergency);
        EXPECT_EQ(ExtractCanCategory(sentId.Get29BitId()), 0x03);
        EXPECT_EQ(ExtractCanMessageType(sentId.Get29BitId()), 0x90);
        EXPECT_EQ(ExtractCanNodeId(sentId.Get29BitId()), 0x042);
        ASSERT_EQ(sentData.size(), 1u);
        EXPECT_EQ(sentData[0], 0x07);
    }

    TEST(CanCategoryTest, CommandAckGoesToTheAcknowledger)
    {
        testing::StrictMock<hal::CanMock> can;
        CanFrameTransport transport{ can, 0x042 };
        FaultReportingCategoryServer category{ transport };
        testing::StrictMock<CanCommandAcknowledgerMock> acknowledger;
        category.SetAcknowledger(acknowledger);

        EXPECT_CALL(acknowledger, SendCommandAck(0x03, 0x01, CanAckStatus::categoryError));
        category.SendCommandAck(0x01, CanAckStatus::categoryError);
    }

    TEST(CanCategoryTest, CommandAckAfterTheAcknowledgerIsClearedIsDropped)
    {
        testing::StrictMock<hal::CanMock> can;
        CanFrameTransport transport{ can, 0x042 };
        FaultReportingCategoryServer category{ transport };
        testing::StrictMock<CanCommandAcknowledgerMock> acknowledger;
        category.SetAcknowledger(acknowledger);
        category.ClearAcknowledger();

        category.SendCommandAck(0x01, CanAckStatus::success);
    }

    TEST(CanCategoryTest, AddingASecondMessageTypeWithTheSameIdAsserts)
    {
        StubCategoryServer category(0x01);
        StubMessageType first(0x10);
        StubMessageType second(0x10);
        category.AddMessageType(first);

        EXPECT_DEATH(category.AddMessageType(second), "");
    }

    TEST(CanCategoryTest, MessageTypesWithDistinctIdsAreAllDispatched)
    {
        StubCategoryServer category(0x01);
        StubMessageType first(0x10);
        StubMessageType second(0x11);
        category.AddMessageTypes(first, second);

        category.HandleMessage(0x10, hal::Can::Message{});
        category.HandleMessage(0x11, hal::Can::Message{});

        EXPECT_EQ(first.handleCallCount, 1);
        EXPECT_EQ(second.handleCallCount, 1);
    }

    TEST(CanCategoryTest, ServerDefaultRequiresSequenceValidation)
    {
        StubCategoryServer category(0x01);
        EXPECT_TRUE(category.RequiresSequenceValidation());
    }

    TEST(CanCategoryTest, ClientDefaultDoesNotRequireSequenceValidation)
    {
        StubCategoryClient category(0x01);
        EXPECT_FALSE(category.RequiresSequenceValidation());
    }

    TEST(CanCategoryTest, IdReturnsConfiguredValue)
    {
        StubCategoryServer cat1(0x01);
        StubCategoryServer cat2(0x05);
        StubCategoryServer cat3(0x0F);

        EXPECT_EQ(cat1.Id(), 0x01);
        EXPECT_EQ(cat2.Id(), 0x05);
        EXPECT_EQ(cat3.Id(), 0x0F);
    }

    TEST(CanCategoryTest, HandleMessageDispatchesToRegisteredMessageType)
    {
        StubCategoryServer category(0x01);
        StubMessageType msg1(0x01);
        StubMessageType msg2(0x02);
        category.AddMessageType(msg1);
        category.AddMessageType(msg2);

        hal::Can::Message data;
        data.push_back(0xAA);
        data.push_back(0xBB);

        EXPECT_EQ(category.HandleMessage(0x01, data), CanDispatchResult::handled);
        EXPECT_EQ(msg1.handleCallCount, 1);
        EXPECT_EQ(msg1.lastDataSize, 2u);
        EXPECT_EQ(msg2.handleCallCount, 0);
    }

    TEST(CanCategoryTest, HandleMessageReturnsFalseForUnknownType)
    {
        StubCategoryServer category(0x01);
        StubMessageType msg1(0x01);
        category.AddMessageType(msg1);

        hal::Can::Message data;
        EXPECT_EQ(category.HandleMessage(0xFF, data), CanDispatchResult::unknownMessageType);
        EXPECT_EQ(msg1.handleCallCount, 0);
    }

    TEST(CanCategoryTest, HandleMessageDispatchesCorrectMessageType)
    {
        StubCategoryServer category(0x01);
        StubMessageType msg1(0x01);
        StubMessageType msg2(0x02);
        category.AddMessageType(msg1);
        category.AddMessageType(msg2);

        hal::Can::Message data;
        EXPECT_EQ(category.HandleMessage(0x02, data), CanDispatchResult::handled);
        EXPECT_EQ(msg1.handleCallCount, 0);
        EXPECT_EQ(msg2.handleCallCount, 1);
    }

    TEST(CanCategoryTest, HandleMessageCanBeCalledMultipleTimes)
    {
        StubCategoryServer category(0x01);
        StubMessageType msg1(0x01);
        category.AddMessageType(msg1);

        hal::Can::Message data;
        category.HandleMessage(0x01, data);
        category.HandleMessage(0x01, data);

        EXPECT_EQ(msg1.handleCallCount, 2);
    }

    TEST(CanCategoryTest, HandlePduMessage_DispatchesToRegisteredHandler)
    {
        StubCategoryServer category(0x01);
        StubPduMessageType msgPdu(0x10);
        category.AddMessageType(msgPdu);

        uint8_t data[] = { 0x01, 0x02, 0x03 };

        EXPECT_EQ(category.HandlePduMessage(0x10, infra::MakeRange(data)), CanDispatchResult::handled);
        EXPECT_EQ(msgPdu.handlePduCallCount, 1);
        EXPECT_EQ(msgPdu.lastPduSize, 3u);
    }

    TEST(CanCategoryTest, HandlePduMessage_ReportsUnknownMessageType)
    {
        StubCategoryServer category(0x01);
        StubPduMessageType msgPdu(0x10);
        category.AddMessageType(msgPdu);

        uint8_t data[] = { 0x01 };

        EXPECT_EQ(category.HandlePduMessage(0xFF, infra::MakeRange(data)), CanDispatchResult::unknownMessageType);
        EXPECT_EQ(msgPdu.handlePduCallCount, 0);
    }

    TEST(CanCategoryTest, HandlePduMessage_DefaultHandlePdu_RejectsRatherThanCrashing)
    {
        StubCategoryServer category(0x01);
        StubMessageType msgDefault(0x20);
        category.AddMessageType(msgDefault);

        uint8_t data[] = { 0xAB };

        EXPECT_EQ(category.HandlePduMessage(0x20, infra::MakeRange(data)), CanDispatchResult::rejected);
    }

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
