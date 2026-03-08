#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace testing;
    using namespace services;

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
            FixtureInit(NiceMock<hal::CanMock>& canMock,
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
        NiceMock<hal::CanMock> canMock;
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
        auto id = MakeSystemId(0xFF);

        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto& cb)
            {
                ASSERT_GE(data.size(), 3u);
                EXPECT_EQ(data[0], canSystemCategoryId);
                EXPECT_EQ(data[1], 0xFF);
                EXPECT_EQ(data[2], static_cast<uint8_t>(CanAckStatus::unknownCommand));
                cb(true);
            });

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
        NiceMock<hal::CanMock> limitedCan;

        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> limitedReceiveCallback;

        EXPECT_CALL(limitedCan, ReceiveData(_)).WillOnce([&limitedReceiveCallback](const auto& callback)
            {
                limitedReceiveCallback = callback;
            });
        ON_CALL(limitedCan, SendData(_, _, _))
            .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
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
        NiceMock<hal::CanMock> limitedCan;

        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> limitedReceiveCallback;

        EXPECT_CALL(limitedCan, ReceiveData(_)).WillOnce([&limitedReceiveCallback](const auto& callback)
            {
                limitedReceiveCallback = callback;
            });
        ON_CALL(limitedCan, SendData(_, _, _))
            .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
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

        class TestCategory : public CanCategory
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

        class TestCategory : public CanCategory
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

        class TestCategory : public CanCategory
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

        class TestCategory : public CanCategory
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

        class TestCategory : public CanCategory
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

    TEST_F(CanProtocolServerTest, RegisterCategory_DuplicateIdAsserts)
    {
        class TestCategory : public CanCategory
        {
        public:
            uint8_t Id() const override
            {
                return canSystemCategoryId;
            }
        };

        TestCategory duplicate;
        EXPECT_DEATH(server.RegisterCategory(duplicate), "");
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
        class TestCategory : public CanCategory
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
}
