#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "can-lite/transport/IsoTpTransportImpl.hpp"
#include "can-lite/testing/EchoCategoryClient.hpp"
#include "can-lite/testing/EchoCategoryServer.hpp"
#include "can-lite/testing/VirtualCan.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>

namespace
{
    using namespace services;
    using testing::ElementsAre;
    using testing::StrictMock;

    class EchoCategoryServerObserverMock
        : public EchoCategoryServerObserver
    {
    public:
        using EchoCategoryServerObserver::EchoCategoryServerObserver;

        MOCK_METHOD(void, OnEchoRequest, (infra::ConstByteRange payload), (override));
        MOCK_METHOD(void, OnValidatedRequest, (infra::ConstByteRange payload), (override));
    };

    class EchoCategoryClientObserverMock
        : public EchoCategoryClientObserver
    {
    public:
        using EchoCategoryClientObserver::EchoCategoryClientObserver;

        MOCK_METHOD(void, OnEchoReply, (infra::ConstByteRange payload), (override));
    };

    class EchoCategoryTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        struct Init
        {
            Init(VirtualCan& serverCan, VirtualCan& clientCan)
            {
                serverCan.ConnectTo(clientCan);
            }
        };

        EchoCategoryTest()
        {
            server.RegisterCategory(echoServer);
            client.RegisterCategory(echoClient);
        }

        ~EchoCategoryTest() override
        {
            client.UnregisterCategory(echoClient);
            server.UnregisterCategory(echoServer);
        }

        static hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message message;
            for (auto byte : bytes)
                message.push_back(byte);
            return message;
        }

        static constexpr uint8_t categoryId = 0x03;
        static constexpr uint16_t serverNodeId = 1;

        VirtualCan serverCan;
        VirtualCan clientCan;
        Init init{ serverCan, clientCan };
        CanProtocolServer::Config config{ serverNodeId, 500, std::chrono::seconds(1) };
        CanProtocolServer server{ serverCan, config };
        CanProtocolClient client{ clientCan };
        EchoCategoryServer echoServer{ categoryId };
        EchoCategoryClient echoClient{ categoryId };
    };

    TEST_F(EchoCategoryTest, Id_IsTheConstructorParameter)
    {
        EXPECT_EQ(echoServer.Id(), categoryId);
        EXPECT_EQ(echoClient.Id(), categoryId);
    }

    TEST_F(EchoCategoryTest, Server_DefaultsToSequenceValidation)
    {
        EXPECT_TRUE(echoServer.RequiresSequenceValidation());
        EXPECT_FALSE(EchoCategoryServer(categoryId, false).RequiresSequenceValidation());
    }

    TEST_F(EchoCategoryTest, Client_DoesNotValidateSequences)
    {
        EXPECT_FALSE(echoClient.RequiresSequenceValidation());
    }

    TEST_F(EchoCategoryTest, EchoRequest_TravelsToTheServerAndBack)
    {
        StrictMock<EchoCategoryServerObserverMock> serverObserver{ echoServer };
        StrictMock<EchoCategoryClientObserverMock> clientObserver{ echoClient };

        EXPECT_CALL(serverObserver, OnEchoRequest(testing::_));
        EXPECT_CALL(clientObserver, OnEchoReply(testing::_));

        EXPECT_TRUE(echoClient.SendEchoRequest(serverNodeId, MakeMessage({ 0xDE, 0xAD })));

        EXPECT_EQ(echoServer.RequestCount(), 1u);
        EXPECT_EQ(echoClient.ReplyCount(), 1u);
    }

    TEST_F(EchoCategoryTest, EchoReply_CarriesTheRequestPayloadIncludingItsSequence)
    {
        echoClient.SendEchoRequest(serverNodeId, MakeMessage({ 0xDE, 0xAD }));

        EXPECT_THAT(clientCan.lastSentData, ElementsAre(0, 0xDE, 0xAD));
        EXPECT_THAT(serverCan.lastSentData, ElementsAre(0, 0xDE, 0xAD));
    }

    TEST_F(EchoCategoryTest, Requests_AreSequencedByTheHost)
    {
        echoClient.SendEchoRequest(serverNodeId, MakeMessage({ 1 }));
        EXPECT_EQ(clientCan.lastSentData[0], 0);

        echoClient.SendEchoRequest(serverNodeId, MakeMessage({ 2 }));
        EXPECT_EQ(clientCan.lastSentData[0], 1);

        echoClient.SendEchoRequest(serverNodeId, MakeMessage({ 3 }));
        EXPECT_EQ(clientCan.lastSentData[0], 2);

        EXPECT_EQ(echoServer.RequestCount(), 3u);
    }

    TEST_F(EchoCategoryTest, ValidatedRequest_AcceptsALongEnoughPayload)
    {
        StrictMock<EchoCategoryServerObserverMock> serverObserver{ echoServer };

        EXPECT_CALL(serverObserver, OnValidatedRequest(testing::_));

        // One sequence byte plus the four payload bytes the handler requires.
        echoClient.SendValidatedRequest(serverNodeId, MakeMessage({ 1, 2, 3, 4 }));

        EXPECT_EQ(echoServer.ValidatedRequestCount(), 1u);
        EXPECT_EQ(echoServer.RejectedRequestCount(), 0u);
    }

    TEST_F(EchoCategoryTest, ValidatedRequest_RejectsAShortPayloadWithInvalidPayloadAck)
    {
        echoClient.SendValidatedRequest(serverNodeId, MakeMessage({ 0xAA }));

        EXPECT_EQ(echoServer.ValidatedRequestCount(), 0u);
        EXPECT_EQ(echoServer.RejectedRequestCount(), 1u);

        ASSERT_EQ(serverCan.lastSentData.size(), canCommandAckSize);
        EXPECT_EQ(serverCan.lastSentData[0], categoryId);
        EXPECT_EQ(serverCan.lastSentData[1], echoValidatedRequestMessageTypeId);
        EXPECT_EQ(serverCan.lastSentData[2], static_cast<uint8_t>(CanAckStatus::invalidPayload));
    }

    TEST_F(EchoCategoryTest, UnknownMessageType_IsAcknowledgedAsUnknownCommand)
    {
        auto id = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::command, categoryId, 0x7F, serverNodeId));

        serverCan.InjectFrame(id, MakeMessage({ 0 }));

        ASSERT_EQ(serverCan.lastSentData.size(), canCommandAckSize);
        EXPECT_EQ(serverCan.lastSentData[1], 0x7F);
        EXPECT_EQ(serverCan.lastSentData[2], static_cast<uint8_t>(CanAckStatus::unknownCommand));
    }

    TEST_F(EchoCategoryTest, OutOfOrderSequence_IsRejectedAndTheClientResynchronises)
    {
        echoClient.SendEchoRequest(serverNodeId, MakeMessage({ 0xAA }));

        // A frame lost on the way to the server desynchronises the two ends.
        auto id = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::command, categoryId, echoRequestMessageTypeId, serverNodeId));
        serverCan.InjectFrame(id, MakeMessage({ 40, 0xBB }));

        EXPECT_EQ(echoServer.RequestCount(), 1u);
        ASSERT_EQ(serverCan.lastSentData.size(), canCommandAckSize);
        EXPECT_EQ(serverCan.lastSentData[2], static_cast<uint8_t>(CanAckStatus::sequenceError));

        // The acknowledgement told the client what the server expects next.
        echoClient.SendEchoRequest(serverNodeId, MakeMessage({ 0xCC }));

        EXPECT_EQ(clientCan.lastSentData[0], 1);
        EXPECT_EQ(echoServer.RequestCount(), 2u);
    }

    TEST_F(EchoCategoryTest, RateLimit_DropsFramesBeyondTheBudget)
    {
        CanProtocolServer::Config limitedConfig{ 2, 2, std::chrono::seconds(1) };
        VirtualCan limitedServerCan;
        VirtualCan limitedClientCan;
        limitedServerCan.ConnectTo(limitedClientCan);

        CanProtocolServer limitedServer{ limitedServerCan, limitedConfig };
        EchoCategoryServer limitedEchoServer{ categoryId, false };
        limitedServer.RegisterCategory(limitedEchoServer);

        auto id = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::command, categoryId, echoRequestMessageTypeId, 2));

        limitedServerCan.InjectFrame(id, MakeMessage({ 1 }));
        limitedServerCan.InjectFrame(id, MakeMessage({ 2 }));
        limitedServerCan.InjectFrame(id, MakeMessage({ 3 }));

        EXPECT_EQ(limitedEchoServer.RequestCount(), 2u);

        limitedServer.UnregisterCategory(limitedEchoServer);
    }

    TEST_F(EchoCategoryTest, Discovery_ReportsTheRegisteredEchoCategory)
    {
        infra::BoundedVector<uint8_t>::WithMaxSize<canMaxCategories> discovered;

        client.DiscoverCategories(serverNodeId, [&discovered](infra::ConstByteRange categories)
            {
                for (auto category : categories)
                    discovered.push_back(category);
            });

        EXPECT_THAT(discovered, ElementsAre(canSystemCategoryId, categoryId));
    }

    TEST_F(EchoCategoryTest, EchoRequest_ArrivesReassembledOverIsoTp)
    {
        // The category handles a byte range, so a payload too long for one
        // frame reaches exactly the same handler once ISO-TP has reassembled it.
        constexpr uint16_t maxPduSize = 64;
        constexpr uint16_t clientNodeId = 2;

        auto dataId = MakeCanId(CanPriority::command, categoryId, echoRequestMessageTypeId, serverNodeId);
        auto flowControlId = MakeCanId(CanPriority::response, categoryId, echoRequestMessageTypeId, clientNodeId);

        IsoTpTransportImpl::WithStorage<maxPduSize> serverIsoTp{ server.Transport() };
        server.AttachIsoTpTransport(serverIsoTp);
        ASSERT_TRUE(serverIsoTp.RegisterReceiveChannel(dataId, flowControlId));

        CanFrameTransport clientTransport{ clientCan, clientNodeId };
        IsoTpTransportImpl::WithStorage<maxPduSize> clientIsoTp{ clientTransport };
        client.AttachIsoTpTransport(clientIsoTp);

        std::array<uint8_t, 20> pdu{};
        pdu[0] = 0;
        for (std::size_t i = 1; i != pdu.size(); ++i)
            pdu[i] = static_cast<uint8_t>(i);

        StrictMock<EchoCategoryServerObserverMock> serverObserver{ echoServer };
        EXPECT_CALL(serverObserver, OnEchoRequest(testing::_)).WillOnce(testing::Invoke([&pdu](infra::ConstByteRange payload)
            {
                EXPECT_EQ(payload.size(), pdu.size());
            }));

        bool sent = false;
        ASSERT_TRUE(clientIsoTp.SendPdu(dataId, flowControlId, infra::MakeRange(pdu), [&sent]
            {
                sent = true;
            }));

        ForwardTime(std::chrono::milliseconds(100));

        EXPECT_TRUE(sent);
        EXPECT_EQ(echoServer.RequestCount(), 1u);
    }
}
