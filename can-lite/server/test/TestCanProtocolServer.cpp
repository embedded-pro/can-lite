#include "can-lite/core/test/CanCategoryStubs.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
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

    class CanProtocolServerObserverMock
        : public CanProtocolServerObserver
    {
    public:
        using CanProtocolServerObserver::CanProtocolServerObserver;

        MOCK_METHOD(void, Online, (), (override));
        MOCK_METHOD(void, Offline, (), (override));
    };

    class CanProtocolServerTest
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

        hal::Can::Id MakeSystemId(uint8_t messageType, uint16_t nodeId = 1)
        {
            return hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::heartbeat, canSystemCategoryId, messageType, nodeId));
        }

        hal::Can::Id MakeCommandId(uint8_t category, uint8_t messageType, uint16_t nodeId = 1)
        {
            return hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::command, category, messageType, nodeId));
        }

        CanProtocolServer::Config config{ 1, 500, std::chrono::seconds(1) };
        StrictMock<hal::CanMock> canMock;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        FixtureInit fixtureInit{ canMock, receiveCallback };

        CanProtocolServer server{ canMock, config };
        StrictMock<CanProtocolServerObserverMock> observerMock{ server };
    };

    TEST_F(CanProtocolServerTest, HeartbeatReceived_NotifiesOnline)
    {
        auto id = MakeSystemId(canHeartbeatMessageTypeId);

        EXPECT_CALL(observerMock, Online());

        SimulateRx(id, MakeMessage({ canProtocolVersion }));
    }

    TEST_F(CanProtocolServerTest, ClientGoesOfflineAfterHeartbeatTimeout)
    {
        auto id = MakeSystemId(canHeartbeatMessageTypeId);

        EXPECT_CALL(observerMock, Online());
        SimulateRx(id, MakeMessage({ canProtocolVersion }));

        EXPECT_CALL(observerMock, Offline());
        ForwardTime(std::chrono::seconds(3));
    }

    TEST_F(CanProtocolServerTest, HeartbeatBeforeTimeout_DefersClientOffline)
    {
        auto id = MakeSystemId(canHeartbeatMessageTypeId);

        EXPECT_CALL(observerMock, Online()).Times(2);
        SimulateRx(id, MakeMessage({ canProtocolVersion }));
        ForwardTime(std::chrono::seconds(2));
        SimulateRx(id, MakeMessage({ canProtocolVersion }));
        ForwardTime(std::chrono::seconds(2));

        EXPECT_CALL(observerMock, Offline());
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(CanProtocolServerTest, NoHeartbeatEverReceived_NeverGoesOffline)
    {
        ForwardTime(std::chrono::seconds(10));
    }

    TEST_F(CanProtocolServerTest, ClientReconnectsAfterTimeout_NotifiesOnlineAgain)
    {
        auto id = MakeSystemId(canHeartbeatMessageTypeId);

        EXPECT_CALL(observerMock, Online());
        SimulateRx(id, MakeMessage({ canProtocolVersion }));

        EXPECT_CALL(observerMock, Offline());
        ForwardTime(std::chrono::seconds(3));

        EXPECT_CALL(observerMock, Online());
        SimulateRx(id, MakeMessage({ canProtocolVersion }));
    }

    TEST_F(CanProtocolServerTest, StatusRequestReceived_SendsHeartbeat)
    {
        auto id = MakeSystemId(canStatusRequestMessageTypeId);

        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
            {
                ASSERT_GE(data.size(), 1u);
                EXPECT_EQ(data[0], canProtocolVersion);
                cb(true);
            });

        SimulateRx(id, MakeMessage({}));
    }

    TEST_F(CanProtocolServerTest, RejectsMessageForDifferentNode)
    {
        auto id = MakeSystemId(canHeartbeatMessageTypeId, 99);

        EXPECT_CALL(observerMock, Online()).Times(0);

        SimulateRx(id, MakeMessage({ canProtocolVersion }));
    }

    TEST_F(CanProtocolServerTest, AcceptsBroadcastMessage)
    {
        auto id = MakeSystemId(canHeartbeatMessageTypeId, canBroadcastNodeId);

        EXPECT_CALL(observerMock, Online());

        SimulateRx(id, MakeMessage({ canProtocolVersion }));
    }

    TEST_F(CanProtocolServerTest, Rejects11BitId)
    {
        auto id = hal::Can::Id::Create11BitId(0x100);

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, UnknownCategory_SilentlyDiscarded)
    {
        uint32_t rawId = MakeCanId(CanPriority::command, 0x0F, 0x01, 1);
        auto id = hal::Can::Id::Create29BitId(rawId);

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, UnknownSystemMessageType_AcksUnknownCommand)
    {
        // 0x7E is within the command range (<= canLastCommandMessageTypeId) but
        // not registered by the System category.
        auto id = MakeSystemId(0x7E);

        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
            {
                ASSERT_GE(data.size(), 3u);
                EXPECT_EQ(data[0], canSystemCategoryId);
                EXPECT_EQ(data[1], 0x7E);
                EXPECT_EQ(data[2], static_cast<uint8_t>(CanAckStatus::unknownCommand));
                cb(true);
            });

        SimulateRx(id, MakeMessage({}));
    }

    TEST_F(CanProtocolServerTest, ResponseRangeMessageType_SilentlyIgnored)
    {
        // 0x80+ is reserved for responses; the server must never treat it as a
        // command (see H-4) even if no handler happens to match it either.
        auto id = MakeSystemId(0x80);

        SimulateRx(id, MakeMessage({}));
    }

    TEST_F(CanProtocolServerTest, HeartbeatTimer_SendsPeriodicHeartbeat)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
            {
                ASSERT_GE(data.size(), 1u);
                EXPECT_EQ(data[0], canProtocolVersion);
                cb(true);
            });

        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(CanProtocolServerTest, RateLimiting_RejectsExcessMessages)
    {
        CanProtocolServer::Config limitedConfig{ 1, 3, std::chrono::seconds(1) };
        StrictMock<hal::CanMock> limitedCan;

        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> limitedReceiveCallback;

        EXPECT_CALL(limitedCan, ReceiveData(_)).WillOnce([&limitedReceiveCallback](const auto& callback)
            {
                limitedReceiveCallback = callback;
            });
        EXPECT_CALL(limitedCan, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
            {
                cb(true);
            }));

        CanProtocolServer limitedServer(limitedCan, limitedConfig);
        StrictMock<CanProtocolServerObserverMock> limitedObserver(limitedServer);

        auto id = MakeSystemId(canHeartbeatMessageTypeId);

        EXPECT_CALL(limitedObserver, Online()).Times(3);

        for (int i = 0; i < 3; ++i)
            limitedReceiveCallback(id, MakeMessage({ canProtocolVersion }));

        limitedReceiveCallback(id, MakeMessage({ canProtocolVersion }));
    }

    TEST_F(CanProtocolServerTest, RateLimiting_ResetsCounterAutomatically)
    {
        CanProtocolServer::Config limitedConfig{ 1, 2, std::chrono::seconds(1) };
        StrictMock<hal::CanMock> limitedCan;

        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> limitedReceiveCallback;

        EXPECT_CALL(limitedCan, ReceiveData(_)).WillOnce([&limitedReceiveCallback](const auto& callback)
            {
                limitedReceiveCallback = callback;
            });
        EXPECT_CALL(limitedCan, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
            {
                cb(true);
            }));

        CanProtocolServer limitedServer(limitedCan, limitedConfig);
        StrictMock<CanProtocolServerObserverMock> limitedObserver(limitedServer);

        auto id = MakeSystemId(canHeartbeatMessageTypeId);

        EXPECT_CALL(limitedObserver, Online()).Times(2);
        limitedReceiveCallback(id, MakeMessage({ canProtocolVersion }));
        limitedReceiveCallback(id, MakeMessage({ canProtocolVersion }));

        limitedReceiveCallback(id, MakeMessage({ canProtocolVersion }));

        EXPECT_CALL(limitedObserver, Online());
        ForwardTime(std::chrono::seconds(1));
        limitedReceiveCallback(id, MakeMessage({ canProtocolVersion }));
    }

    // REQ-CAN-006.1: one server serves exactly one client, so a second client's
    // interleaved stream breaks the shared sequence counter by design.
    TEST_F(CanProtocolServerTest, SequenceValidation_RejectsInterleavedSecondClient)
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

        class TestCategory : public CanCategoryServerStub
        {
        public:
            TestCategory()
            {
                AddMessageType(msg);
            }

            uint8_t Id() const override
            {
                return 0x01;
            }

            TestMessageType msg;
        };

        TestCategory testCategory;
        server.RegisterCategory(testCategory);

        auto id = MakeCommandId(0x01, 0x01);

        SimulateRx(id, MakeMessage({ 10 }));
        SimulateRx(id, MakeMessage({ 11 }));
        EXPECT_EQ(testCategory.msg.handleCount, 2);

        EXPECT_CALL(canMock, SendData(_, _, _));
        SimulateRx(id, MakeMessage({ 200 }));
        EXPECT_EQ(testCategory.msg.handleCount, 2);

        server.UnregisterCategory(testCategory);
    }

    TEST_F(CanProtocolServerTest, SequenceValidation_RejectsDuplicateOnRegisteredCategory)
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

        class TestCategory : public CanCategoryServerStub
        {
        public:
            TestCategory()
            {
                AddMessageType(msg);
            }

            uint8_t Id() const override
            {
                return 0x01;
            }

            bool RequiresSequenceValidation() const override
            {
                return true;
            }

            TestMessageType msg;
        };

        TestCategory testCategory;
        server.RegisterCategory(testCategory);

        auto id = MakeCommandId(0x01, 0x01);

        SimulateRx(id, MakeMessage({ 1 }));
        EXPECT_EQ(testCategory.msg.handleCount, 1);

        EXPECT_CALL(canMock, SendData(_, _, _));
        SimulateRx(id, MakeMessage({ 1 }));
        EXPECT_EQ(testCategory.msg.handleCount, 1);

        server.UnregisterCategory(testCategory);
    }

    TEST_F(CanProtocolServerTest, SequenceValidation_AcceptsSequentialMessages)
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

        class TestCategory : public CanCategoryServerStub
        {
        public:
            TestCategory()
            {
                AddMessageType(msg);
            }

            uint8_t Id() const override
            {
                return 0x01;
            }

            bool RequiresSequenceValidation() const override
            {
                return true;
            }

            TestMessageType msg;
        };

        TestCategory testCategory;
        server.RegisterCategory(testCategory);

        auto id = MakeCommandId(0x01, 0x01);

        SimulateRx(id, MakeMessage({ 1 }));
        SimulateRx(id, MakeMessage({ 2 }));

        EXPECT_EQ(testCategory.msg.handleCount, 2);

        server.UnregisterCategory(testCategory);
    }

    TEST_F(CanProtocolServerTest, SequenceWrapsAround)
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

        class TestCategory : public CanCategoryServerStub
        {
        public:
            TestCategory()
            {
                AddMessageType(msg);
            }

            uint8_t Id() const override
            {
                return 0x01;
            }

            bool RequiresSequenceValidation() const override
            {
                return true;
            }

            TestMessageType msg;
        };

        TestCategory testCategory;
        server.RegisterCategory(testCategory);

        auto id = MakeCommandId(0x01, 0x01);

        SimulateRx(id, MakeMessage({ 255 }));
        SimulateRx(id, MakeMessage({ 0 }));

        EXPECT_EQ(testCategory.msg.handleCount, 2);

        server.UnregisterCategory(testCategory);
    }

    TEST_F(CanProtocolServerTest, EmptyPayload_SequenceProtectedCommand_Rejected)
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

        class TestCategory : public CanCategoryServerStub
        {
        public:
            TestCategory()
            {
                AddMessageType(msg);
            }

            uint8_t Id() const override
            {
                return 0x01;
            }

            bool RequiresSequenceValidation() const override
            {
                return true;
            }

            TestMessageType msg;
        };

        TestCategory testCategory;
        server.RegisterCategory(testCategory);

        auto id = MakeCommandId(0x01, 0x01);

        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({}));
        EXPECT_EQ(testCategory.msg.handleCount, 0);

        server.UnregisterCategory(testCategory);
    }

    TEST_F(CanProtocolServerTest, SystemCategoryDoesNotRequireSequenceValidation)
    {
        auto id = MakeSystemId(canStatusRequestMessageTypeId);

        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
            {
                cb(true);
            });

        SimulateRx(id, MakeMessage({}));
    }

    TEST_F(CanProtocolServerTest, RegisterCategory_DispatchesMessages)
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

        class TestCategory : public CanCategoryServerStub
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
        server.RegisterCategory(testCategory);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x05, 0x42, 1);
        auto id = hal::Can::Id::Create29BitId(rawId);

        SimulateRx(id, MakeMessage({ 0xAA }));

        EXPECT_TRUE(testCategory.msg.handled);

        server.UnregisterCategory(testCategory);
    }

    TEST_F(CanProtocolServerTest, RegisterCategory_DuplicateIdReturnsFalse)
    {
        class TestCategory : public CanCategoryServerStub
        {
        public:
            uint8_t Id() const override
            {
                return canSystemCategoryId;
            }
        };

        TestCategory duplicate;
        EXPECT_FALSE(server.RegisterCategory(duplicate));
    }

    TEST_F(CanProtocolServerTest, RegisterCategory_AtCapacityReturnsFalse)
    {
        class TestCategory : public CanCategoryServerStub
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
            EXPECT_TRUE(server.RegisterCategory(category));

        TestCategory oneTooMany(canMaxRegisteredCategories);
        EXPECT_FALSE(server.RegisterCategory(oneTooMany));
    }

    TEST_F(CanProtocolServerTest, Construct_WithDefaultConstructedConfig_AssertsInsteadOfBroadcasting)
    {
        StrictMock<hal::CanMock> unconfiguredCan;
        EXPECT_CALL(unconfiguredCan, ReceiveData(_)).Times(AnyNumber());

        EXPECT_DEATH(CanProtocolServer(unconfiguredCan, CanProtocolServer::Config{}), "");
    }

    TEST_F(CanProtocolServerTest, ConstructorAutoRegistersReceiveCallback)
    {
        StrictMock<hal::CanMock> testCan;

        EXPECT_CALL(testCan, ReceiveData(_));
        ON_CALL(testCan, SendData(_, _, _))
            .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                {
                    cb(true);
                }));

        CanProtocolServer testServer(testCan, config);
    }

    TEST_F(CanProtocolServerTest, CategoryListRequest_RespondsWithRegisteredCategories)
    {
        class TestCategory : public CanCategoryServerStub
        {
        public:
            explicit TestCategory(uint8_t id)
                : categoryId(id)
            {}

            uint8_t Id() const override
            {
                return categoryId;
            }

            bool RequiresSequenceValidation() const override
            {
                return false;
            }

        private:
            uint8_t categoryId;
        };

        TestCategory cat1(0x01);
        TestCategory cat2(0x05);
        server.RegisterCategory(cat1);
        server.RegisterCategory(cat2);

        hal::Can::Message capturedData;
        hal::Can::Id capturedId = hal::Can::Id::Create29BitId(0);
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(DoAll(SaveArg<0>(&capturedId), SaveArg<1>(&capturedData), Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                                                                                                                       {
                                                                                                                           cb(true);
                                                                                                                       })));

        auto id = MakeSystemId(canCategoryListRequestMessageTypeId);
        SimulateRx(id, MakeMessage({}));

        uint32_t rawId = capturedId.Get29BitId();
        EXPECT_EQ(ExtractCanCategory(rawId), canSystemCategoryId);
        EXPECT_EQ(ExtractCanMessageType(rawId), canCategoryListResponseMessageTypeId);
        EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::response);

        ASSERT_EQ(capturedData.size(), 3u);
        EXPECT_EQ(capturedData[0], canSystemCategoryId);
        EXPECT_EQ(capturedData[1], 0x01);
        EXPECT_EQ(capturedData[2], 0x05);

        server.UnregisterCategory(cat2);
        server.UnregisterCategory(cat1);
    }

    TEST_F(CanProtocolServerTest, HeartbeatTimer_DeferredWhileCommunicating)
    {
        auto statusRequestId = MakeSystemId(canStatusRequestMessageTypeId);

        int heartbeatCount = 0;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillRepeatedly([&heartbeatCount](hal::Can::Id id, const hal::Can::Message&, const auto& cb)
            {
                uint32_t rawId = id.Get29BitId();
                if (ExtractCanCategory(rawId) == canSystemCategoryId && ExtractCanMessageType(rawId) == canHeartbeatMessageTypeId)
                    ++heartbeatCount;
                cb(true);
            });

        // At t=800ms, receive a status request — server sends a heartbeat and resets the timer
        ForwardTime(std::chrono::milliseconds(800));
        SimulateRx(statusRequestId, MakeMessage({}));
        EXPECT_EQ(heartbeatCount, 1);

        // At t=1000ms, the original timer would have fired — but it was deferred to t=1800ms
        ForwardTime(std::chrono::milliseconds(200));
        EXPECT_EQ(heartbeatCount, 1);

        // At t=1800ms, the deferred heartbeat fires
        ForwardTime(std::chrono::milliseconds(800));
        EXPECT_EQ(heartbeatCount, 2);
    }

    // === ISO-TP transport integration ===

    TEST_F(CanProtocolServerTest, AttachIsoTpTransport_ProcessFrameInterceptsMessage)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        auto id = hal::Can::Id::Create29BitId(MakeCanId(CanPriority::command, 0x01, 0x01, 1));
        EXPECT_CALL(mockIsoTp, ProcessFrame(_, _)).WillOnce(Return(true));

        SimulateRx(id, MakeMessage({ 0x01 }));
    }

    TEST_F(CanProtocolServerTest, AttachIsoTpTransport_OnAbort_ReleasesChannel)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_));
        infra::Function<void(uint32_t, iso_tp::AbortReason)> capturedOnAbort;
        EXPECT_CALL(mockIsoTp, SetOnAbort(_)).WillOnce(SaveArg<0>(&capturedOnAbort));
        server.AttachIsoTpTransport(mockIsoTp);

        EXPECT_CALL(mockIsoTp, ReleaseChannel(0x321u));
        capturedOnAbort(0x321u, iso_tp::AbortReason::overflow);
    }

    TEST_F(CanProtocolServerTest, AttachIsoTpTransport_ProcessFrameReturnsFalse_ContinuesNormalDispatch)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        auto id = MakeSystemId(canHeartbeatMessageTypeId);
        EXPECT_CALL(mockIsoTp, ProcessFrame(_, _)).WillOnce(Return(false));
        EXPECT_CALL(observerMock, Online());

        SimulateRx(id, MakeMessage({ canProtocolVersion }));
    }

    TEST_F(CanProtocolServerTest, AttachIsoTpTransport_DispatchesPduToCategory)
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

        class PduCategory : public CanCategoryServerStub
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
        server.RegisterCategory(pduCategory);

        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x05, 0x42, 1);
        uint8_t pduData[] = { 0xDE, 0xAD };
        capturedPduCallback(rawId, infra::MakeRange(pduData));

        EXPECT_TRUE(pduCategory.msg.pduReceived);
        server.UnregisterCategory(pduCategory);
    }

    TEST_F(CanProtocolServerTest, AttachIsoTpTransport_DispatchPdu_UnknownCategory_Ignored)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x0F, 0x01, 1);
        uint8_t pduData[] = { 0xAA };
        capturedPduCallback(rawId, infra::MakeRange(pduData));
    }

    TEST_F(CanProtocolServerTest, DispatchPdu_ResponseRangeMessageType_SilentlyIgnored)
    {
        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, canSystemCategoryId, 0x80, 1);
        uint8_t pduData[] = { 0xAA };
        capturedPduCallback(rawId, infra::MakeRange(pduData));
    }

    TEST_F(CanProtocolServerTest, DispatchPdu_RateLimited_SilentlyDiscarded)
    {
        CanProtocolServer::Config limitedConfig{ 1, 1, std::chrono::seconds(1) };
        StrictMock<hal::CanMock> limitedCan;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> limitedReceiveCallback;

        EXPECT_CALL(limitedCan, ReceiveData(_)).WillOnce([&limitedReceiveCallback](const auto& callback)
            {
                limitedReceiveCallback = callback;
            });
        EXPECT_CALL(limitedCan, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
            {
                cb(true);
            }));

        CanProtocolServer limitedServer(limitedCan, limitedConfig);

        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        limitedServer.AttachIsoTpTransport(mockIsoTp);

        EXPECT_CALL(mockIsoTp, ProcessFrame(_, _)).WillOnce(Return(false));
        limitedReceiveCallback(MakeSystemId(canHeartbeatMessageTypeId), MakeMessage({ canProtocolVersion }));

        uint32_t rawId = MakeCanId(CanPriority::command, canSystemCategoryId, canStatusRequestMessageTypeId, 1);
        capturedPduCallback(rawId, infra::ConstByteRange{});
    }

    class SequenceValidatedPduCategory : public CanCategoryServerStub
    {
    public:
        SequenceValidatedPduCategory()
        {
            AddMessageType(msg);
        }

        uint8_t Id() const override
        {
            return 0x06;
        }

        class PduMessageType : public CanMessageType
        {
        public:
            uint8_t Id() const override
            {
                return 0x50;
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

        PduMessageType msg;
    };

    TEST_F(CanProtocolServerTest, DispatchPdu_SequenceValidatedCategory_EmptyPayload_SendsInvalidPayloadAck)
    {
        SequenceValidatedPduCategory category;
        server.RegisterCategory(category);

        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
            {
                ASSERT_GE(data.size(), 3u);
                EXPECT_EQ(data[0], 0x06);
                EXPECT_EQ(data[1], 0x50);
                EXPECT_EQ(data[2], static_cast<uint8_t>(CanAckStatus::invalidPayload));
                cb(true);
            });

        uint32_t rawId = MakeCanId(CanPriority::command, 0x06, 0x50, 1);
        capturedPduCallback(rawId, infra::ConstByteRange{});

        EXPECT_FALSE(category.msg.pduReceived);
        server.UnregisterCategory(category);
    }

    TEST_F(CanProtocolServerTest, DispatchPdu_SequenceValidatedCategory_SequenceError_SendsAck)
    {
        SequenceValidatedPduCategory category;
        server.RegisterCategory(category);

        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x06, 0x50, 1);

        uint8_t firstPayload[] = { 0x00 };
        capturedPduCallback(rawId, infra::MakeRange(firstPayload));
        ASSERT_TRUE(category.msg.pduReceived);

        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
            {
                ASSERT_GE(data.size(), 4u);
                EXPECT_EQ(data[2], static_cast<uint8_t>(CanAckStatus::sequenceError));
                EXPECT_EQ(data[3], 1u);
                cb(true);
            });

        uint8_t wrongSequencePayload[] = { 0x05 };
        capturedPduCallback(rawId, infra::MakeRange(wrongSequencePayload));

        server.UnregisterCategory(category);
    }

    TEST_F(CanProtocolServerTest, Transport_ReturnsTransportRef)
    {
        CanFrameTransport& t = server.Transport();
        EXPECT_EQ(&t, &server.Transport());
    }

    TEST_F(CanProtocolServerTest, DispatchPdu_WrongNodeId_Ignored)
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

        class PduCategory : public CanCategoryServerStub
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
        server.RegisterCategory(pduCategory);

        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        // nodeId=2 != config.nodeId(1) and != canBroadcastNodeId(0)
        uint32_t rawId = MakeCanId(CanPriority::command, 0x05, 0x42, 2);
        uint8_t pduData[] = { 0xDE };
        capturedPduCallback(rawId, infra::MakeRange(pduData));

        EXPECT_FALSE(pduCategory.msg.pduReceived);
        server.UnregisterCategory(pduCategory);
    }

    TEST_F(CanProtocolServerTest, DispatchPdu_BroadcastNodeId_Accepted)
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

        class PduCategory : public CanCategoryServerStub
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
        server.RegisterCategory(pduCategory);

        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x05, 0x42, canBroadcastNodeId);
        uint8_t pduData[] = { 0xDE, 0xAD };
        capturedPduCallback(rawId, infra::MakeRange(pduData));

        EXPECT_TRUE(pduCategory.msg.pduReceived);
        server.UnregisterCategory(pduCategory);
    }

    TEST_F(CanProtocolServerTest, DispatchPdu_UnknownMessageType_SendsUnknownCommandAck)
    {
        class EmptyCategory : public CanCategoryServerStub
        {
        public:
            uint8_t Id() const override
            {
                return 0x05;
            }

            bool RequiresSequenceValidation() const override
            {
                return false;
            }
        };

        EmptyCategory emptyCategory;
        server.RegisterCategory(emptyCategory);

        StrictMock<MockIsoTpTransport> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        EXPECT_CALL(mockIsoTp, SetOnAbort(_));
        server.AttachIsoTpTransport(mockIsoTp);

        // message type 0x7E is within the command range but not registered in emptyCategory
        uint32_t rawId = MakeCanId(CanPriority::command, 0x05, 0x7E, 1);
        uint8_t pduData[] = { 0xAA };

        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
            {
                ASSERT_GE(data.size(), 3u);
                EXPECT_EQ(data[0], 0x05);
                EXPECT_EQ(data[1], 0x7E);
                EXPECT_EQ(data[2], static_cast<uint8_t>(CanAckStatus::unknownCommand));
                cb(true);
            });

        capturedPduCallback(rawId, infra::MakeRange(pduData));
        server.UnregisterCategory(emptyCategory);
    }
}
