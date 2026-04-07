#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/transport/IsoTpTransport.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace testing;
    using namespace services;

    class MockIsoTpTransport : public IsoTpTransport
    {
    public:
        MOCK_METHOD(bool, RegisterReceiveChannel, (uint32_t, uint32_t), (override));
        MOCK_METHOD(bool, SendPdu, (uint32_t, uint32_t, infra::ConstByteRange, const infra::Function<void()>&), (override));
        MOCK_METHOD(bool, ProcessFrame, (uint32_t, const hal::Can::Message&), (override));
        MOCK_METHOD(void, SetOnPduReceived, (infra::Function<void(uint32_t, infra::ConstByteRange)>), (override));
        MOCK_METHOD(void, SetOnAbort, (infra::Function<void(uint32_t, iso_tp::AbortReason)>), (override));
    };

    class CanProtocolClientTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        struct FixtureInit
        {
            FixtureInit(StrictMock<hal::CanMock>& canMock,
                infra::Function<void(hal::Can::Id, const hal::Can::Message&)>& receiveCallback)
            {
                EXPECT_CALL(canMock, ReceiveData(_)).WillOnce([&receiveCallback](const auto& callback)
                    {
                        receiveCallback = callback;
                    });
                ON_CALL(canMock, SendData(_, _, _))
                    .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                        {
                            cb(true);
                        }));
            }
        };

        void SimulateRx(hal::Can::Id id, const hal::Can::Message& data)
        {
            receiveCallback(id, data);
        }

        hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message msg;
            for (auto b : bytes)
                msg.push_back(b);
            return msg;
        }

        hal::Can::Id MakeSystemId(uint8_t messageType, uint16_t nodeId = 0)
        {
            return hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::response, canSystemCategoryId, messageType, nodeId));
        }

        StrictMock<hal::CanMock> canMock;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        FixtureInit fixtureInit{ canMock, receiveCallback };
        CanProtocolClient client{ canMock };
    };

    // === RegisterCategory / UnregisterCategory ===

    TEST_F(CanProtocolClientTest, RegisterCategory_DispatchesReceivedMessages)
    {
        class TestMessageType : public CanMessageType
        {
        public:
            uint8_t Id() const override
            {
                return 0x42;
            }

            void Handle(const hal::Can::Message& data) override
            {
                handled = true;
            }

            bool handled = false;
        };

        class TestCategory : public CanCategoryClient
        {
        public:
            TestCategory()
            {
                AddMessageType(msg);
            }

            uint8_t Id() const override
            {
                return 0x05;
            }

            bool RequiresSequenceValidation() const override
            {
                return false;
            }

            TestMessageType msg;
        };

        TestCategory testCategory;
        client.RegisterCategory(testCategory);

        uint32_t rawId = MakeCanId(CanPriority::telemetry, 0x05, 0x42, 0);
        auto id = hal::Can::Id::Create29BitId(rawId);

        SimulateRx(id, MakeMessage({ 0xAA }));

        EXPECT_TRUE(testCategory.msg.handled);

        client.UnregisterCategory(testCategory);
    }

    TEST_F(CanProtocolClientTest, RegisterCategory_DuplicateIdAsserts)
    {
        class TestCategory : public CanCategoryClient
        {
        public:
            uint8_t Id() const override
            {
                return canSystemCategoryId;
            }
        };

        TestCategory duplicate;
        EXPECT_DEATH(client.RegisterCategory(duplicate), "");
    }

    TEST_F(CanProtocolClientTest, UnregisterCategory_StopsDispatch)
    {
        class TestMessageType : public CanMessageType
        {
        public:
            uint8_t Id() const override
            {
                return 0x01;
            }

            void Handle(const hal::Can::Message&) override
            {
                handleCount++;
            }

            int handleCount = 0;
        };

        class TestCategory : public CanCategoryClient
        {
        public:
            TestCategory()
            {
                AddMessageType(msg);
            }

            uint8_t Id() const override
            {
                return 0x03;
            }

            bool RequiresSequenceValidation() const override
            {
                return false;
            }

            TestMessageType msg;
        };

        TestCategory testCategory;
        client.RegisterCategory(testCategory);

        uint32_t rawId = MakeCanId(CanPriority::telemetry, 0x03, 0x01, 0);
        auto id = hal::Can::Id::Create29BitId(rawId);

        SimulateRx(id, MakeMessage({ 0xAA }));
        EXPECT_EQ(testCategory.msg.handleCount, 1);

        client.UnregisterCategory(testCategory);

        SimulateRx(id, MakeMessage({ 0xBB }));
        EXPECT_EQ(testCategory.msg.handleCount, 1);
    }

    // === DiscoverCategories ===

    TEST_F(CanProtocolClientTest, DiscoverCategories_SendsRequestToNodeId)
    {
        hal::Can::Id capturedId = hal::Can::Id::Create29BitId(0);
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(DoAll(SaveArg<0>(&capturedId), Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                                                                                            {
                                                                                                cb(true);
                                                                                            })));

        client.DiscoverCategories(42, [](const hal::Can::Message&) {});

        EXPECT_TRUE(capturedId.Is29BitId());
        uint32_t rawId = capturedId.Get29BitId();
        EXPECT_EQ(ExtractCanCategory(rawId), canSystemCategoryId);
        EXPECT_EQ(ExtractCanMessageType(rawId), canCategoryListRequestMessageTypeId);
        EXPECT_EQ(ExtractCanNodeId(rawId), 42u);
        EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::command);
    }

    TEST_F(CanProtocolClientTest, DiscoverCategories_ResponseInvokesCallback)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));

        client.DiscoverCategories(1, [](const hal::Can::Message& categories)
            {
                ASSERT_EQ(categories.size(), 3u);
                EXPECT_EQ(categories[0], 0x00);
                EXPECT_EQ(categories[1], 0x01);
                EXPECT_EQ(categories[2], 0x05);
            });

        auto responseId = MakeSystemId(canCategoryListResponseMessageTypeId);
        SimulateRx(responseId, MakeMessage({ 0x00, 0x01, 0x05 }));
    }

    TEST_F(CanProtocolClientTest, DiscoverCategories_ResponseWithoutPendingRequest_Ignored)
    {
        auto responseId = MakeSystemId(canCategoryListResponseMessageTypeId);
        SimulateRx(responseId, MakeMessage({ 0x00, 0x01 }));
    }

    TEST_F(CanProtocolClientTest, DiscoverCategories_CallbackClearedAfterResponse)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));

        int callCount = 0;
        client.DiscoverCategories(1, [&callCount](const hal::Can::Message&)
            {
                callCount++;
            });

        auto responseId = MakeSystemId(canCategoryListResponseMessageTypeId);
        SimulateRx(responseId, MakeMessage({ 0x00 }));
        EXPECT_EQ(callCount, 1);

        SimulateRx(responseId, MakeMessage({ 0x00 }));
        EXPECT_EQ(callCount, 1);
    }

    // === Edge cases ===

    TEST_F(CanProtocolClientTest, IgnoresStandard11BitId)
    {
        auto id = hal::Can::Id::Create11BitId(0x100);
        SimulateRx(id, MakeMessage({ 0x01 }));
    }

    TEST_F(CanProtocolClientTest, UnknownCategory_SilentlyIgnored)
    {
        uint32_t rawId = MakeCanId(CanPriority::command, 0x0F, 0x01, 0);
        auto id = hal::Can::Id::Create29BitId(rawId);

        SimulateRx(id, MakeMessage({ 0xAA }));
    }

    TEST_F(CanProtocolClientTest, UnknownSystemMessageType_SilentlyIgnored)
    {
        auto id = MakeSystemId(0xFF);
        SimulateRx(id, MakeMessage({ 0x01 }));
    }

    TEST_F(CanProtocolClientTest, ConstructorAutoRegistersReceiveCallback)
    {
        StrictMock<hal::CanMock> testCan;

        EXPECT_CALL(testCan, ReceiveData(_));
        ON_CALL(testCan, SendData(_, _, _))
            .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                {
                    cb(true);
                }));

        CanProtocolClient testClient(testCan);
    }

    // === PeekSequence / CommitSequence ===

    TEST_F(CanProtocolClientTest, PeekSequence_FirstCallReturnsZero)
    {
        EXPECT_EQ(client.PeekSequence(1), 0u);
    }

    TEST_F(CanProtocolClientTest, PeekSequence_DoesNotAdvanceWithoutCommit)
    {
        client.PeekSequence(1);
        EXPECT_EQ(client.PeekSequence(1), 0u);
    }

    TEST_F(CanProtocolClientTest, CommitSequence_AdvancesCounter)
    {
        client.PeekSequence(1);
        client.CommitSequence(1);
        EXPECT_EQ(client.PeekSequence(1), 1u);
    }

    TEST_F(CanProtocolClientTest, PeekCommitSequence_IndependentPerServer)
    {
        EXPECT_EQ(client.PeekSequence(1), 0u);
        client.CommitSequence(1);
        EXPECT_EQ(client.PeekSequence(2), 0u);
        client.CommitSequence(2);
        EXPECT_EQ(client.PeekSequence(1), 1u);
        client.CommitSequence(1);
        EXPECT_EQ(client.PeekSequence(2), 1u);
    }

    // === Server liveness ===

    class CanProtocolClientObserverMock
        : public CanProtocolClientObserver
    {
    public:
        using CanProtocolClientObserver::CanProtocolClientObserver;

        MOCK_METHOD(void, OnServerOnline, (uint16_t nodeId), (override));
        MOCK_METHOD(void, OnServerOffline, (uint16_t nodeId), (override));
    };

    class CanProtocolClientLivenessTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        struct FixtureInit
        {
            FixtureInit(hal::CanMock& canMock,
                infra::Function<void(hal::Can::Id, const hal::Can::Message&)>& receiveCallback)
            {
                EXPECT_CALL(canMock, ReceiveData(_)).WillOnce([&receiveCallback](const auto& callback)
                    {
                        receiveCallback = callback;
                    });
                EXPECT_CALL(canMock, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    }));
            }
        };

        void SimulateServerFrame(uint16_t sourceNodeId)
        {
            uint32_t rawId = MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, sourceNodeId);
            auto id = hal::Can::Id::Create29BitId(rawId);
            receiveCallback(id, hal::Can::Message{});
        }

        StrictMock<hal::CanMock> canMock;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        FixtureInit fixtureInit{ canMock, receiveCallback };
        CanProtocolClient::Config config{ std::chrono::seconds(3) };
        CanProtocolClient client{ canMock, config };
        StrictMock<CanProtocolClientObserverMock> observer{ client };
    };

    TEST_F(CanProtocolClientLivenessTest, ReceivedFrame_NotifiesServerOnline)
    {
        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateServerFrame(5);
    }

    TEST_F(CanProtocolClientLivenessTest, TwoFramesFromSameServer_NotifiesOnlineOnce)
    {
        EXPECT_CALL(observer, OnServerOnline(5u)).Times(1);
        SimulateServerFrame(5);
        SimulateServerFrame(5);
    }

    TEST_F(CanProtocolClientLivenessTest, ServerGoesOfflineAfterTimeout)
    {
        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateServerFrame(5);

        EXPECT_CALL(observer, OnServerOffline(5u));
        ForwardTime(std::chrono::seconds(3));
    }

    TEST_F(CanProtocolClientLivenessTest, FrameBeforeTimeoutDefersOffline)
    {
        EXPECT_CALL(observer, OnServerOnline(5u)).Times(1);
        SimulateServerFrame(5);
        ForwardTime(std::chrono::seconds(2));
        SimulateServerFrame(5);
        ForwardTime(std::chrono::seconds(2));

        EXPECT_CALL(observer, OnServerOffline(5u));
        ForwardTime(std::chrono::seconds(1));
    }

    // === ISO-TP transport integration ===

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_ProcessFrameInterceptsMessage)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_));
        client.AttachIsoTpTransport(mockIsoTp);

        auto id = hal::Can::Id::Create29BitId(MakeCanId(CanPriority::command, 0x01, 0x01, 0));
        EXPECT_CALL(mockIsoTp, ProcessFrame(_, _)).WillOnce(Return(true));

        SimulateRx(id, MakeMessage({ 0x01 }));
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_ProcessFrameReturnsFalse_ContinuesNormalDispatch)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_));
        client.AttachIsoTpTransport(mockIsoTp);

        auto id = hal::Can::Id::Create29BitId(MakeCanId(CanPriority::command, 0x0F, 0x01, 0));
        EXPECT_CALL(mockIsoTp, ProcessFrame(_, _)).WillOnce(Return(false));

        SimulateRx(id, MakeMessage({ 0xAA }));
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_DispatchesPduToCategory)
    {
        class PduMessageType : public CanMessageType
        {
        public:
            uint8_t Id() const override
            {
                return 0x42;
            }

            void Handle(const hal::Can::Message&) override
            {}

            bool HandlePdu(infra::ConstByteRange) override
            {
                pduReceived = true;
                return true;
            }

            bool pduReceived = false;
        };

        class PduCategory : public CanCategoryClient
        {
        public:
            PduCategory()
            {
                AddMessageType(msg);
            }

            uint8_t Id() const override
            {
                return 0x05;
            }

            bool RequiresSequenceValidation() const override
            {
                return false;
            }

            PduMessageType msg;
        };

        PduCategory pduCategory;
        client.RegisterCategory(pduCategory);

        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        client.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x05, 0x42, 0);
        uint8_t pduData[] = { 0xDE, 0xAD };
        capturedPduCallback(rawId, infra::MakeRange(pduData));

        EXPECT_TRUE(pduCategory.msg.pduReceived);
        client.UnregisterCategory(pduCategory);
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_DispatchPdu_UnknownCategory_Ignored)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        client.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x0F, 0x01, 0);
        uint8_t pduData[] = { 0xAA };
        capturedPduCallback(rawId, infra::MakeRange(pduData));
    }
}
