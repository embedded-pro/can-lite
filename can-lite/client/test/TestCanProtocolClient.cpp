#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/test/CanCategoryStubs.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/transport/IsoTpTransport.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>

namespace
{
    using namespace testing;
    using namespace services;

    class MockIsoTpTransport : public IsoTpTransport
    {
    public:
        MOCK_METHOD(bool, RegisterReceiveChannel, (uint32_t, uint32_t), (override));
        MOCK_METHOD(void, ReleaseChannel, (uint32_t), (override));
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

        class TestCategory : public CanCategoryClientStub
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

    TEST_F(CanProtocolClientTest, RegisterCategory_DuplicateIdReturnsFalse)
    {
        class TestCategory : public CanCategoryClientStub
        {
        public:
            uint8_t Id() const override
            {
                return canSystemCategoryId;
            }
        };

        TestCategory duplicate;
        EXPECT_FALSE(client.RegisterCategory(duplicate));
    }

    TEST_F(CanProtocolClientTest, RegisterCategory_AtCapacityReturnsFalse)
    {
        class TestCategory : public CanCategoryClientStub
        {
        public:
            explicit TestCategory(uint8_t id)
                : id(id)
            {}

            uint8_t Id() const override
            {
                return id;
            }

        private:
            uint8_t id;
        };

        std::array<TestCategory, canMaxRegisteredCategories - 1> categories{
            TestCategory(1), TestCategory(2), TestCategory(3), TestCategory(4),
            TestCategory(5), TestCategory(6), TestCategory(7)
        };
        for (auto& category : categories)
            EXPECT_TRUE(client.RegisterCategory(category));

        TestCategory oneTooMany(canMaxRegisteredCategories);
        EXPECT_FALSE(client.RegisterCategory(oneTooMany));
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

        class TestCategory : public CanCategoryClientStub
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
        client.CommitSequence(1, 3, 0x10);
        EXPECT_EQ(client.PeekSequence(1), 1u);
    }

    TEST_F(CanProtocolClientTest, PeekCommitSequence_IndependentPerServer)
    {
        EXPECT_EQ(client.PeekSequence(1), 0u);
        client.CommitSequence(1, 3, 0x10);
        EXPECT_EQ(client.PeekSequence(2), 0u);
        client.CommitSequence(2, 3, 0x10);
        EXPECT_EQ(client.PeekSequence(1), 1u);
        client.CommitSequence(1, 3, 0x10);
        EXPECT_EQ(client.PeekSequence(2), 1u);
    }

    TEST_F(CanProtocolClientTest, CommitSequence_UntrackedNodeAsserts)
    {
        EXPECT_DEATH(client.CommitSequence(42, 3, 0x10), "");
    }

    TEST_F(CanProtocolClientTest, PeekSequence_AllSlotsFull_EvictsOldestRoundRobin)
    {
        for (uint16_t node = 1; node <= 8; ++node)
        {
            client.PeekSequence(node);
            client.CommitSequence(node, 3, 0x10);
        }

        EXPECT_EQ(client.PeekSequence(9), 0u);
        client.CommitSequence(9, 3, 0x10);
        EXPECT_EQ(client.PeekSequence(9), 1u);

        EXPECT_EQ(client.PeekSequence(8), 1u);

        EXPECT_EQ(client.PeekSequence(1), 0u);
    }

    TEST_F(CanProtocolClientTest, SystemCategory_ReturnsSystemCategoryRef)
    {
        EXPECT_EQ(client.SystemCategory().Id(), canSystemCategoryId);
        EXPECT_EQ(&client.SystemCategory(), &client.SystemCategory());
    }

    TEST_F(CanProtocolClientTest, Transport_ReturnsTransportRef)
    {
        EXPECT_EQ(&client.Transport(), &client.Transport());
    }

    // === Server liveness ===

    class CanProtocolClientObserverMock
        : public CanProtocolClientObserver
    {
    public:
        using CanProtocolClientObserver::CanProtocolClientObserver;

        MOCK_METHOD(void, OnServerOnline, (uint16_t nodeId), (override));
        MOCK_METHOD(void, OnServerOffline, (uint16_t nodeId), (override));
        MOCK_METHOD(void, OnCommandAckTimeout, (uint16_t nodeId, uint8_t category, uint8_t messageType), (override));
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

        void SimulateCommandAck(uint16_t sourceNodeId, uint8_t category, uint8_t messageType, CanAckStatus status, uint8_t expectedSequence)
        {
            uint32_t rawId = MakeCanId(CanPriority::response, canSystemCategoryId, canCommandAckMessageTypeId, sourceNodeId);
            auto id = hal::Can::Id::Create29BitId(rawId);
            hal::Can::Message msg;
            msg.push_back(category);
            msg.push_back(messageType);
            msg.push_back(static_cast<uint8_t>(status));
            msg.push_back(expectedSequence);
            receiveCallback(id, msg);
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

    TEST_F(CanProtocolClientLivenessTest, ServerAfterTimeout_ReconnectIsTreatedAsFreshOnline)
    {
        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateServerFrame(5);

        EXPECT_CALL(observer, OnServerOffline(5u));
        ForwardTime(std::chrono::seconds(3));

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateServerFrame(5);
    }

    TEST_F(CanProtocolClientLivenessTest, AllSlotsFull_EvictsOldestRoundRobin)
    {
        for (uint16_t node = 1; node <= 8; ++node)
        {
            EXPECT_CALL(observer, OnServerOnline(node));
            SimulateServerFrame(node);
        }

        {
            InSequence seq;
            EXPECT_CALL(observer, OnServerOffline(1u));
            EXPECT_CALL(observer, OnServerOnline(9u));
        }
        SimulateServerFrame(9);

        ForwardTime(std::chrono::seconds(2));
        for (uint16_t node = 2; node <= 8; ++node)
            SimulateServerFrame(node);

        EXPECT_CALL(observer, OnServerOffline(9u));
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(CanProtocolClientLivenessTest, SequenceErrorAck_ResyncsToServersExpectedSequence)
    {
        client.PeekSequence(5);
        client.CommitSequence(5, 3, 0x10);
        EXPECT_EQ(client.PeekSequence(5), 1u);

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateCommandAck(5, 3, 0x10, CanAckStatus::sequenceError, 7);

        EXPECT_EQ(client.PeekSequence(5), 7u);
    }

    TEST_F(CanProtocolClientLivenessTest, SuccessAck_DoesNotResync)
    {
        client.PeekSequence(5);
        client.CommitSequence(5, 3, 0x10);

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateCommandAck(5, 3, 0x10, CanAckStatus::success, 0);

        EXPECT_EQ(client.PeekSequence(5), 1u);
    }

    TEST_F(CanProtocolClientLivenessTest, CommandAckTimeout_FiresWhenNoAckArrives)
    {
        client.PeekSequence(5);
        client.CommitSequence(5, 3, 0x10);

        EXPECT_CALL(observer, OnCommandAckTimeout(5u, 3u, 0x10u));
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(CanProtocolClientLivenessTest, CommandAckTimeout_DoesNotFireWhenAckArrives)
    {
        client.PeekSequence(5);
        client.CommitSequence(5, 3, 0x10);

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateCommandAck(5, 3, 0x10, CanAckStatus::success, 0);

        ForwardTime(std::chrono::seconds(2));
    }

    TEST_F(CanProtocolClientLivenessTest, MismatchedAck_DoesNotClearAwaitingTimer)
    {
        client.PeekSequence(5);
        client.CommitSequence(5, 3, 0x10);

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateCommandAck(5, 4, 0x20, CanAckStatus::success, 0);

        EXPECT_CALL(observer, OnCommandAckTimeout(5u, 3u, 0x10u));
        ForwardTime(std::chrono::seconds(1));
    }

    // === ISO-TP transport integration ===

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_ProcessFrameInterceptsMessage)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        client.AttachIsoTpTransport(mockIsoTp);

        auto id = hal::Can::Id::Create29BitId(MakeCanId(CanPriority::command, 0x01, 0x01, 0));
        EXPECT_CALL(mockIsoTp, ProcessFrame(_, _)).WillOnce(Return(true));

        SimulateRx(id, MakeMessage({ 0x01 }));
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_OnAbort_ReleasesChannel)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_));
        infra::Function<void(uint32_t, iso_tp::AbortReason)> capturedOnAbort;
        EXPECT_CALL(mockIsoTp, SetOnAbort(_)).WillOnce(SaveArg<0>(&capturedOnAbort));
        client.AttachIsoTpTransport(mockIsoTp);

        EXPECT_CALL(mockIsoTp, ReleaseChannel(0x123u));
        capturedOnAbort(0x123u, iso_tp::AbortReason::nBsTimeout);
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_ProcessFrameReturnsFalse_ContinuesNormalDispatch)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
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

        class PduCategory : public CanCategoryClientStub
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
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        client.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x05, 0x42, 0);
        uint8_t pduData[] = { 0xDE, 0xAD };
        capturedPduCallback(rawId, infra::MakeRange(pduData));

        EXPECT_TRUE(pduCategory.msg.pduReceived);
        client.UnregisterCategory(pduCategory);
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_DispatchPdu_NonzeroSourceNodeMarksServerAlive)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        client.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::response, canSystemCategoryId, canCategoryListResponseMessageTypeId, 7);
        uint8_t pduData[] = { 0x00 };
        capturedPduCallback(rawId, infra::MakeRange(pduData));
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_DispatchPdu_UnknownCategory_Ignored)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        client.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x0F, 0x01, 0);
        uint8_t pduData[] = { 0xAA };
        capturedPduCallback(rawId, infra::MakeRange(pduData));
    }
}
