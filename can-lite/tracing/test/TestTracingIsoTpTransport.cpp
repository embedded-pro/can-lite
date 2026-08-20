#include "can-lite/tracing/TracingIsoTpTransport.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "services/tracer/Tracer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::DoAll;
    using testing::Return;
    using testing::SaveArg;
    using testing::StrictMock;

    class IsoTpTransportMock
        : public IsoTpTransport
    {
    public:
        MOCK_METHOD(bool, RegisterReceiveChannel, (uint32_t dataId, uint32_t fcId), (override));
        MOCK_METHOD(void, ReleaseChannel, (uint32_t dataId), (override));
        MOCK_METHOD(bool, SendPdu, (uint32_t dataId, uint32_t fcId, infra::ConstByteRange pdu, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(bool, ProcessFrame, (uint32_t canId, const hal::Can::Message& frame), (override));
        MOCK_METHOD(void, SetOnPduReceived, (infra::Function<void(uint32_t dataId, infra::ConstByteRange pdu)> callback), (override));
        MOCK_METHOD(void, SetOnAbort, (infra::Function<void(uint32_t dataId, iso_tp::AbortReason reason)> callback), (override));
    };

    hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
    {
        hal::Can::Message message;
        for (auto byte : bytes)
            message.push_back(byte);
        return message;
    }

    class TracingIsoTpTransportTest
        : public testing::Test
    {
    public:
        static constexpr uint32_t dataId = 0x18db33f1;
        static constexpr uint32_t fcId = 0x18da33f1;

        infra::StringOutputStream::WithStorage<512> stream;
        TracerToStream tracer{ stream };
        StrictMock<IsoTpTransportMock> transport;
        TracingIsoTpTransport tracing{ transport, tracer };

        infra::Function<void(uint32_t, infra::ConstByteRange)> pduReceived;
        infra::Function<void(uint32_t, iso_tp::AbortReason)> aborted;

        void RegisterPduReceived()
        {
            EXPECT_CALL(transport, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&pduReceived));
            tracing.SetOnPduReceived([this](uint32_t id, infra::ConstByteRange pdu)
                {
                    forwardedDataId = id;
                    forwardedSize = pdu.size();
                    ++forwardCount;
                });
        }

        void RegisterAborted()
        {
            EXPECT_CALL(transport, SetOnAbort(_)).WillOnce(SaveArg<0>(&aborted));
            tracing.SetOnAbort([this](uint32_t id, iso_tp::AbortReason reason)
                {
                    forwardedDataId = id;
                    forwardedReason = reason;
                    ++forwardCount;
                });
        }

        uint32_t forwardedDataId = 0;
        std::size_t forwardedSize = 0;
        iso_tp::AbortReason forwardedReason = iso_tp::AbortReason::overflow;
        uint32_t forwardCount = 0;
    };

    TEST_F(TracingIsoTpTransportTest, RegisterReceiveChannelForwardsAndTracesAcceptance)
    {
        EXPECT_CALL(transport, RegisterReceiveChannel(dataId, fcId)).WillOnce(Return(true));

        EXPECT_TRUE(tracing.RegisterReceiveChannel(dataId, fcId));

        EXPECT_EQ("\r\nTracingIsoTpTransport: RegisterReceiveChannel dataId 0x18db33f1 fcId 0x18da33f1 accepted", stream.Storage());
    }

    TEST_F(TracingIsoTpTransportTest, RejectedRegisterReceiveChannelIsTraced)
    {
        EXPECT_CALL(transport, RegisterReceiveChannel(dataId, fcId)).WillOnce(Return(false));

        EXPECT_FALSE(tracing.RegisterReceiveChannel(dataId, fcId));

        EXPECT_EQ("\r\nTracingIsoTpTransport: RegisterReceiveChannel dataId 0x18db33f1 fcId 0x18da33f1 rejected", stream.Storage());
    }

    TEST_F(TracingIsoTpTransportTest, ReleaseChannelForwardsAndTraces)
    {
        EXPECT_CALL(transport, ReleaseChannel(dataId));

        tracing.ReleaseChannel(dataId);

        EXPECT_EQ("\r\nTracingIsoTpTransport: ReleaseChannel dataId 0x18db33f1", stream.Storage());
    }

    TEST_F(TracingIsoTpTransportTest, SendPduForwardsAndTracesSize)
    {
        auto pdu = MakeMessage({ 0x01, 0x02, 0x03, 0x04 });
        EXPECT_CALL(transport, SendPdu(dataId, fcId, _, _)).WillOnce(Return(true));

        EXPECT_TRUE(tracing.SendPdu(dataId, fcId, infra::MakeRange(pdu), []() {}));

        EXPECT_EQ("\r\nTracingIsoTpTransport: SendPdu dataId 0x18db33f1 fcId 0x18da33f1 size 0x4 accepted", stream.Storage());
    }

    TEST_F(TracingIsoTpTransportTest, RejectedSendPduIsTraced)
    {
        auto pdu = MakeMessage({ 0x01 });
        EXPECT_CALL(transport, SendPdu(dataId, fcId, _, _)).WillOnce(Return(false));

        EXPECT_FALSE(tracing.SendPdu(dataId, fcId, infra::MakeRange(pdu), []() {}));

        EXPECT_EQ("\r\nTracingIsoTpTransport: SendPdu dataId 0x18db33f1 fcId 0x18da33f1 size 0x1 rejected", stream.Storage());
    }

    TEST_F(TracingIsoTpTransportTest, SendPduCompletionIsForwardedUnwrappedAndUntraced)
    {
        auto pdu = MakeMessage({ 0x01 });
        infra::Function<void()> captured;
        EXPECT_CALL(transport, SendPdu(dataId, fcId, _, _)).WillOnce(DoAll(SaveArg<3>(&captured), Return(true)));
        bool completed = false;

        tracing.SendPdu(dataId, fcId, infra::MakeRange(pdu), [&completed]()
            {
                completed = true;
            });
        auto lengthAfterSend = stream.Storage().size();
        captured();

        EXPECT_TRUE(completed);
        EXPECT_EQ(lengthAfterSend, stream.Storage().size());
    }

    TEST_F(TracingIsoTpTransportTest, ClaimedFrameIsTraced)
    {
        EXPECT_CALL(transport, ProcessFrame(fcId, _)).WillOnce(Return(true));

        EXPECT_TRUE(tracing.ProcessFrame(fcId, MakeMessage({ 0x10, 0x20 })));

        EXPECT_EQ("\r\nTracingIsoTpTransport: ProcessFrame canId 0x18da33f1 dlc 2 data 1020", stream.Storage());
    }

    TEST_F(TracingIsoTpTransportTest, UnclaimedFrameIsSilent)
    {
        EXPECT_CALL(transport, ProcessFrame(fcId, _)).WillOnce(Return(false));

        EXPECT_FALSE(tracing.ProcessFrame(fcId, MakeMessage({ 0x10, 0x20 })));

        EXPECT_TRUE(stream.Storage().empty());
    }

    TEST_F(TracingIsoTpTransportTest, ReceivedPduIsTracedAndForwarded)
    {
        RegisterPduReceived();
        auto pdu = MakeMessage({ 0x01, 0x02, 0x03 });

        pduReceived(dataId, infra::MakeRange(pdu));

        EXPECT_EQ("\r\nTracingIsoTpTransport: PduReceived dataId 0x18db33f1 size 0x3 data 010203", stream.Storage());
        EXPECT_EQ(1u, forwardCount);
        EXPECT_EQ(dataId, forwardedDataId);
        EXPECT_EQ(3u, forwardedSize);
    }

    TEST_F(TracingIsoTpTransportTest, AbortIsTracedAndForwarded)
    {
        RegisterAborted();

        aborted(dataId, iso_tp::AbortReason::nCrTimeout);

        EXPECT_EQ("\r\nTracingIsoTpTransport: Abort dataId 0x18db33f1 reason nCrTimeout", stream.Storage());
        EXPECT_EQ(1u, forwardCount);
        EXPECT_EQ(dataId, forwardedDataId);
        EXPECT_EQ(iso_tp::AbortReason::nCrTimeout, forwardedReason);
    }

    TEST_F(TracingIsoTpTransportTest, EveryAbortReasonIsNamed)
    {
        RegisterAborted();

        aborted(dataId, iso_tp::AbortReason::nBsTimeout);
        aborted(dataId, iso_tp::AbortReason::nCrTimeout);
        aborted(dataId, iso_tp::AbortReason::overflow);
        aborted(dataId, iso_tp::AbortReason::unexpectedFrame);
        aborted(dataId, iso_tp::AbortReason::waitLimitExceeded);

        EXPECT_EQ("\r\nTracingIsoTpTransport: Abort dataId 0x18db33f1 reason nBsTimeout"
                  "\r\nTracingIsoTpTransport: Abort dataId 0x18db33f1 reason nCrTimeout"
                  "\r\nTracingIsoTpTransport: Abort dataId 0x18db33f1 reason overflow"
                  "\r\nTracingIsoTpTransport: Abort dataId 0x18db33f1 reason unexpectedFrame"
                  "\r\nTracingIsoTpTransport: Abort dataId 0x18db33f1 reason waitLimitExceeded",
            stream.Storage());
        EXPECT_EQ(5u, forwardCount);
    }
}
