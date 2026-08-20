#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/tracing/TracingCan.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "services/tracer/Tracer.hpp"
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

    class TracingCanTest
        : public testing::Test
    {
    public:
        infra::StringOutputStream::WithStorage<512> stream;
        TracerToStream tracer{ stream };
        StrictMock<hal::CanMock> can;
        TracingCan tracing{ can, tracer };

        infra::Function<void(bool)> completion;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> received;
        hal::Can::Id sentId{ hal::Can::Id::Create29BitId(0) };
        hal::Can::Message sentData;

        void ExpectOneSend()
        {
            EXPECT_CALL(can, SendData(_, _, _))
                .WillOnce(DoAll(SaveArg<0>(&sentId), SaveArg<1>(&sentData), SaveArg<2>(&completion)));
        }

        void CompleteSend(bool success)
        {
            auto action = completion;
            completion = nullptr;
            action(success);
        }

        void RegisterReceive()
        {
            EXPECT_CALL(can, ReceiveData(_)).WillOnce(SaveArg<0>(&received));
            tracing.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
                {
                    forwardedId = id;
                    forwardedData = data;
                    ++forwardCount;
                });
        }

        hal::Can::Id forwardedId{ hal::Can::Id::Create29BitId(0) };
        hal::Can::Message forwardedData;
        uint32_t forwardCount = 0;
    };

    TEST_F(TracingCanTest, SendDataIsForwardedToDelegate)
    {
        ExpectOneSend();
        auto id = hal::Can::Id::Create29BitId(MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, 0x123));

        tracing.SendData(id, MakeMessage({ 0x01 }), [](bool) {});

        EXPECT_EQ(sentId, id);
        EXPECT_EQ(sentData, MakeMessage({ 0x01 }));
        CompleteSend(true);
    }

    TEST_F(TracingCanTest, SendDataTracesDecodedExtendedFrame)
    {
        ExpectOneSend();

        tracing.SendData(hal::Can::Id::Create29BitId(MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, 0x123)),
            MakeMessage({ 0x01 }), [](bool) {});

        EXPECT_EQ("\r\nTracingCan: TX id 0x10001123 prio heartbeat cat 0x0 system type 0x1 heartbeat node 0x123 dlc 1 data 01", stream.Storage());
        CompleteSend(true);
    }

    TEST_F(TracingCanTest, SendDataTracesStandardFrame)
    {
        ExpectOneSend();

        tracing.SendData(hal::Can::Id::Create11BitId(0x123), MakeMessage({ 0x01, 0x02 }), [](bool) {});

        EXPECT_EQ("\r\nTracingCan: TX standard id 0x123 dlc 2 data 0102", stream.Storage());
        CompleteSend(true);
    }

    TEST_F(TracingCanTest, SuccessfulCompletionIsForwardedAndAddsNoTrace)
    {
        ExpectOneSend();
        bool completed = false;
        bool result = false;

        tracing.SendData(hal::Can::Id::Create11BitId(0x123), MakeMessage({}), [&completed, &result](bool success)
            {
                completed = true;
                result = success;
            });
        CompleteSend(true);

        EXPECT_TRUE(completed);
        EXPECT_TRUE(result);
        EXPECT_EQ("\r\nTracingCan: TX standard id 0x123 dlc 0 data ", stream.Storage());
    }

    TEST_F(TracingCanTest, FailedCompletionIsForwardedAndAddsNoTrace)
    {
        ExpectOneSend();
        bool completed = false;
        bool result = true;

        tracing.SendData(hal::Can::Id::Create11BitId(0x123), MakeMessage({}), [&completed, &result](bool success)
            {
                completed = true;
                result = success;
            });
        CompleteSend(false);

        EXPECT_TRUE(completed);
        EXPECT_FALSE(result);
        EXPECT_EQ("\r\nTracingCan: TX standard id 0x123 dlc 0 data ", stream.Storage());
    }

    TEST_F(TracingCanTest, ConcurrentSendsForwardEachCompletionToItsOwnCaller)
    {
        infra::Function<void(bool)> firstCompletion;
        EXPECT_CALL(can, SendData(_, _, _))
            .WillOnce(SaveArg<2>(&firstCompletion))
            .WillOnce(SaveArg<2>(&completion));
        bool firstCompleted = false;
        bool secondCompleted = false;

        tracing.SendData(hal::Can::Id::Create11BitId(0x123), MakeMessage({}), [&firstCompleted](bool)
            {
                firstCompleted = true;
            });
        tracing.SendData(hal::Can::Id::Create11BitId(0x124), MakeMessage({}), [&secondCompleted](bool)
            {
                secondCompleted = true;
            });

        completion(true);
        firstCompletion(true);

        EXPECT_TRUE(firstCompleted);
        EXPECT_TRUE(secondCompleted);
    }

    TEST_F(TracingCanTest, ReceiveDataRegistersWithDelegate)
    {
        RegisterReceive();

        EXPECT_TRUE(static_cast<bool>(received));
    }

    TEST_F(TracingCanTest, ReceivedFrameIsTracedAndForwarded)
    {
        RegisterReceive();
        auto id = hal::Can::Id::Create29BitId(MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, 0x123));

        received(id, MakeMessage({ 0x01 }));

        EXPECT_EQ("\r\nTracingCan: RX id 0x10001123 prio heartbeat cat 0x0 system type 0x1 heartbeat node 0x123 dlc 1 data 01", stream.Storage());
        EXPECT_EQ(1u, forwardCount);
        EXPECT_EQ(forwardedId, id);
        EXPECT_EQ(forwardedData, MakeMessage({ 0x01 }));
    }

    TEST_F(TracingCanTest, CommandAckFrameIsDecodedOnItsOwnLine)
    {
        RegisterReceive();

        received(hal::Can::Id::Create29BitId(MakeCanId(CanPriority::response, canSystemCategoryId, canCommandAckMessageTypeId, 0x123)),
            MakeMessage({ 0x03, 0x05, 0x04, 0x07 }));

        EXPECT_EQ("\r\nTracingCan: RX id 0x8002123 prio response cat 0x0 system type 0x2 commandAck node 0x123 dlc 4 data 03050407"
                  "\r\nTracingCan: ack cat 0x3 type 0x5 status sequenceError expectedSeq 0x7",
            stream.Storage());
    }

    TEST_F(TracingCanTest, TruncatedCommandAckIsNotDecoded)
    {
        RegisterReceive();

        received(hal::Can::Id::Create29BitId(MakeCanId(CanPriority::response, canSystemCategoryId, canCommandAckMessageTypeId, 0x123)),
            MakeMessage({ 0x03, 0x05, 0x04 }));

        EXPECT_EQ("\r\nTracingCan: RX id 0x8002123 prio response cat 0x0 system type 0x2 commandAck node 0x123 dlc 3 data 030504", stream.Storage());
    }

    TEST_F(TracingCanTest, ApplicationCategoryAndUnknownMessageTypeAreNamedGenerically)
    {
        ExpectOneSend();

        tracing.SendData(hal::Can::Id::Create29BitId(MakeCanId(CanPriority::command, 0x3, 0x42, 0x123)),
            MakeMessage({ 0xaa }), [](bool) {});

        EXPECT_EQ("\r\nTracingCan: TX id 0x4342123 prio command cat 0x3 application type 0x42 unknown node 0x123 dlc 1 data aa", stream.Storage());
        CompleteSend(true);
    }

    TEST_F(TracingCanTest, CategoryErrorMessageTypeIsNamedForAnyCategory)
    {
        RegisterReceive();

        received(hal::Can::Id::Create29BitId(MakeCanId(CanPriority::response, 0x3, canCategoryErrorResponseMessageTypeId, 0x123)),
            MakeMessage({ 0x01 }));

        EXPECT_EQ("\r\nTracingCan: RX id 0x83fe123 prio response cat 0x3 application type 0xfe categoryError node 0x123 dlc 1 data 01", stream.Storage());
    }

    TEST_F(TracingCanTest, FirmwareUpgradeCategoryIsNamed)
    {
        RegisterReceive();

        received(hal::Can::Id::Create29BitId(MakeCanId(CanPriority::command, canFirmwareUpgradeCategoryId, 0x01, 0x123)),
            MakeMessage({ 0x01 }));

        EXPECT_EQ("\r\nTracingCan: RX id 0x4101123 prio command cat 0x1 firmwareUpgrade type 0x1 unknown node 0x123 dlc 1 data 01", stream.Storage());
    }

    TEST_F(TracingCanTest, EmptyPayloadTracesZeroLengthData)
    {
        ExpectOneSend();

        tracing.SendData(hal::Can::Id::Create29BitId(MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, 0x123)),
            MakeMessage({}), [](bool) {});

        EXPECT_EQ("\r\nTracingCan: TX id 0x10001123 prio heartbeat cat 0x0 system type 0x1 heartbeat node 0x123 dlc 0 data ", stream.Storage());
        CompleteSend(true);
    }
}
