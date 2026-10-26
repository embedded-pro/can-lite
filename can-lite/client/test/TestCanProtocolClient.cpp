#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/client/test/CanProtocolClientObserverMock.hpp"
#include "can-lite/core/test/CanCategoryStubs.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/transport/IsoTpTransport.hpp"
#include "can-lite/transport/test/IsoTpTransportMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <optional>

namespace
{
    using namespace testing;
    using namespace services;

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
                EXPECT_CALL(canMock, ReceiveData(_)).Times(2).WillRepeatedly([&receiveCallback](const auto& callback)
                    {
                        receiveCallback = callback;
                    });
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

    TEST_F(CanProtocolClientTest, RegisterCategory_DispatchesReceivedMessages)
    {
        class TestMessageType : public CanMessageType
        {
        public:
            uint8_t Id() const override
            {
                return 0x42;
            }

            bool Handle(const hal::Can::Message& data) override
            {
                handled = true;
                return true;
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

        for (auto& category : categories)
            client.UnregisterCategory(category);
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

            bool Handle(const hal::Can::Message&) override
            {
                handleCount++;
                return true;
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

    TEST_F(CanProtocolClientTest, TransportIsReadableThroughAConstClientAndAddressesNoNodeOfItsOwn)
    {
        const CanProtocolClient& constClient = client;

        EXPECT_EQ(constClient.Transport().NodeId(), canBroadcastNodeId);
    }

    class CanProtocolClientDiscoveryTest
        : public CanProtocolClientTest
    {
    public:
        void ExpectRequest(uint16_t nodeId)
        {
            EXPECT_CALL(canMock, SendData(hal::Can::Id::Create29BitId(MakeCanId(CanPriority::command, canSystemCategoryId, canCategoryListRequestMessageTypeId, nodeId)), IsEmpty(), _))
                .WillOnce(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    }));
        }

        void ExpectHeartbeats(int n)
        {
            auto id = hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, canBroadcastNodeId));
            EXPECT_CALL(canMock, SendData(id, ElementsAre(canProtocolVersion), _))
                .Times(n)
                .WillRepeatedly(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    }));
        }

        infra::Function<void(std::optional<infra::ConstByteRange>)> Record()
        {
            return [this](std::optional<infra::ConstByteRange> categoryIds)
            {
                ++completions;
                answered = categoryIds.has_value();
                received.clear();
                if (categoryIds)
                    for (auto id : *categoryIds)
                        received.push_back(id);
            };
        }

        int completions = 0;
        bool answered = false;
        hal::Can::Message received;
    };

    TEST_F(CanProtocolClientDiscoveryTest, SendsAnEmptyRequestToTheNode)
    {
        ExpectRequest(42);

        EXPECT_TRUE(client.DiscoverCategories(42, Record()));
    }

    TEST_F(CanProtocolClientDiscoveryTest, AnswerFromTheRequestedNodeCompletesIt)
    {
        ExpectRequest(1);
        client.DiscoverCategories(1, Record());

        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 1), MakeMessage({ 0x00, 0x01, 0x05 }));

        EXPECT_EQ(completions, 1);
        EXPECT_TRUE(answered);
        EXPECT_EQ(received, MakeMessage({ 0x00, 0x01, 0x05 }));
    }

    TEST_F(CanProtocolClientDiscoveryTest, AnswerFromAnotherNodeIsIgnored)
    {
        ExpectRequest(1);
        client.DiscoverCategories(1, Record());

        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 2), MakeMessage({ 0x00, 0x07 }));
        EXPECT_EQ(completions, 0);

        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 1), MakeMessage({ 0x00 }));
        EXPECT_EQ(completions, 1);
        EXPECT_EQ(received, MakeMessage({ 0x00 }));
    }

    TEST_F(CanProtocolClientDiscoveryTest, AnswerWithoutAPendingRequestIsIgnored)
    {
        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 1), MakeMessage({ 0x00, 0x01 }));

        EXPECT_EQ(completions, 0);
    }

    TEST_F(CanProtocolClientDiscoveryTest, CompletesOnlyOnce)
    {
        ExpectRequest(1);
        client.DiscoverCategories(1, Record());

        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 1), MakeMessage({ 0x00 }));
        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 1), MakeMessage({ 0x00 }));

        EXPECT_EQ(completions, 1);
    }

    TEST_F(CanProtocolClientDiscoveryTest, SecondRequestWhileOneIsPendingIsRefused)
    {
        ExpectRequest(1);
        client.DiscoverCategories(1, Record());

        int secondCompletions = 0;
        EXPECT_FALSE(client.DiscoverCategories(2, [&secondCompletions](std::optional<infra::ConstByteRange>)
            {
                ++secondCompletions;
            }));

        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 1), MakeMessage({ 0x00 }));

        EXPECT_EQ(completions, 1);
        EXPECT_EQ(secondCompletions, 0);
    }

    TEST_F(CanProtocolClientDiscoveryTest, BroadcastRequestIsRefused)
    {
        EXPECT_FALSE(client.DiscoverCategories(canBroadcastNodeId, Record()));

        ExpectHeartbeats(1);
        ForwardTime(std::chrono::seconds(1));
        EXPECT_EQ(completions, 0);
    }

    TEST_F(CanProtocolClientDiscoveryTest, CompletesWithoutAnAnswerAfterTheTimeout)
    {
        ExpectRequest(1);
        client.DiscoverCategories(1, Record());

        ForwardTime(std::chrono::milliseconds(999));
        EXPECT_EQ(completions, 0);

        ExpectHeartbeats(1);
        ForwardTime(std::chrono::milliseconds(1));
        EXPECT_EQ(completions, 1);
        EXPECT_FALSE(answered);

        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 1), MakeMessage({ 0x00 }));
        EXPECT_EQ(completions, 1);
    }

    TEST_F(CanProtocolClientDiscoveryTest, AnswerCancelsTheTimeout)
    {
        ExpectRequest(1);
        client.DiscoverCategories(1, Record());
        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 1), MakeMessage({ 0x00 }));

        ExpectHeartbeats(2);
        ForwardTime(std::chrono::seconds(2));

        EXPECT_EQ(completions, 1);
        EXPECT_TRUE(answered);
    }

    TEST_F(CanProtocolClientDiscoveryTest, CompletionMayStartTheNextDiscovery)
    {
        ExpectRequest(1);
        ExpectRequest(2);
        bool secondAccepted = false;
        client.DiscoverCategories(1, [this, &secondAccepted](std::optional<infra::ConstByteRange>)
            {
                secondAccepted = client.DiscoverCategories(2, Record());
            });

        SimulateRx(MakeSystemId(canCategoryListResponseMessageTypeId, 1), MakeMessage({ 0x00 }));

        EXPECT_TRUE(secondAccepted);
    }

    TEST_F(CanProtocolClientDiscoveryTest, RequestTheTransportRefusesLeavesNothingPending)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).Times(1);
        while (client.Transport().SendFrame(7, CanPriority::command, 0x05, 0x01, MakeMessage({ 0x00 }), [](bool) {}))
        {
        }

        EXPECT_FALSE(client.DiscoverCategories(1, Record()));

        ForwardTime(std::chrono::seconds(2));
        EXPECT_EQ(completions, 0);
    }

    TEST_F(CanProtocolClientDiscoveryTest, AnswerReassembledByIsoTpCompletesIt)
    {
        StrictMock<IsoTpTransportMock> isoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> pduCallback;
        EXPECT_CALL(isoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&pduCallback));
        client.AttachIsoTpTransport(isoTp);

        ExpectRequest(1);
        client.DiscoverCategories(1, Record());

        std::array<uint8_t, 2> categoryIds{ 0x00, 0x04 };
        pduCallback(MakeCanId(CanPriority::response, canSystemCategoryId, canCategoryListResponseMessageTypeId, 1), infra::MakeRange(categoryIds));

        EXPECT_EQ(received, MakeMessage({ 0x00, 0x04 }));

        EXPECT_CALL(isoTp, SetOnPduReceived(_));
        client.DetachIsoTpTransport();
    }

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

    TEST_F(CanProtocolClientTest, ConstructorRegistersAndDestructorDeregistersReceiveCallback)
    {
        StrictMock<hal::CanMock> testCan;

        bool registered = false;
        bool deregistered = false;

        EXPECT_CALL(testCan, ReceiveData(_)).Times(2).WillRepeatedly([&registered, &deregistered](const infra::Function<void(hal::Can::Id, const hal::Can::Message&)>& callback)
            {
                if (callback)
                    registered = true;
                else
                    deregistered = true;
            });

        {
            CanProtocolClient testClient(testCan);
            EXPECT_TRUE(registered);
            EXPECT_FALSE(deregistered);
        }

        EXPECT_TRUE(deregistered);
    }

    TEST_F(CanProtocolClientTest, ReserveSequence_FirstReservationIsZero)
    {
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 0u);
    }

    TEST_F(CanProtocolClientTest, ReserveSequence_AdvancesWithEachReservation)
    {
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 0u);
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 1u);
    }

    TEST_F(CanProtocolClientTest, ReserveSequence_IndependentPerServer)
    {
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 0u);
        EXPECT_EQ(client.ReserveSequence(2, 3, 0x10), 0u);
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 1u);
        EXPECT_EQ(client.ReserveSequence(2, 3, 0x10), 1u);
    }

    TEST_F(CanProtocolClientTest, ReserveSequence_AllSlotsFull_EvictsOldestRoundRobin)
    {
        for (uint16_t node = 1; node <= 8; ++node)
            client.ReserveSequence(node, 3, 0x10);

        EXPECT_EQ(client.ReserveSequence(9, 3, 0x10), 0u);
        EXPECT_EQ(client.ReserveSequence(9, 3, 0x10), 1u);
        EXPECT_EQ(client.ReserveSequence(8, 3, 0x10), 1u);
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 0u);
    }

    TEST_F(CanProtocolClientTest, CancelReservation_TheNumberIsReused)
    {
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 0u);
        client.CancelReservation(1, 0);

        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 0u);
    }

    TEST_F(CanProtocolClientTest, CancelReservation_RestoresTheEvictedServer)
    {
        for (uint16_t node = 1; node <= 8; ++node)
            client.ReserveSequence(node, 3, 0x10);

        EXPECT_EQ(client.ReserveSequence(9, 3, 0x10), 0u);
        client.CancelReservation(9, 0);

        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 1u);
    }

    TEST_F(CanProtocolClientTest, CancelReservation_OtherThanTheLatestAsserts)
    {
        client.ReserveSequence(1, 3, 0x10);

        EXPECT_DEATH(client.CancelReservation(2, 0), "");
    }

    TEST_F(CanProtocolClientTest, ReturnSequence_RollsBackWhenNoLaterCommandTookANumber)
    {
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 0u);
        client.ReturnSequence(1, 0);

        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 0u);
    }

    TEST_F(CanProtocolClientTest, ReturnSequence_AfterALaterReservationKeepsTheCounter)
    {
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 0u);
        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 1u);
        client.ReturnSequence(1, 0);

        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 2u);
    }

    TEST_F(CanProtocolClientTest, ReturnSequence_WrapsAround)
    {
        for (int i = 0; i < 255; ++i)
            client.ReserveSequence(1, 3, 0x10);

        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 255u);
        client.ReturnSequence(1, 255);

        EXPECT_EQ(client.ReserveSequence(1, 3, 0x10), 255u);
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

    class CommandingCategory
        : public CanCategoryClient
    {
    public:
        CommandingCategory(CanFrameTransport& transport, CanSequenceSource& sequenceSource)
            : CanCategoryClient(transport, sequenceSource)
        {}

        uint8_t Id() const override
        {
            return 3;
        }

        bool Send(uint16_t targetNodeId)
        {
            return SendCommand(targetNodeId, 0x10);
        }
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
                EXPECT_CALL(canMock, ReceiveData(_)).Times(2).WillRepeatedly([&receiveCallback](const auto& callback)
                    {
                        receiveCallback = callback;
                    });
            }
        };

        void ExpectHeartbeats(int n)
        {
            auto id = hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, canBroadcastNodeId));
            EXPECT_CALL(canMock, SendData(id, ElementsAre(canProtocolVersion), _))
                .Times(n)
                .WillRepeatedly(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    }));
        }

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

        ExpectHeartbeats(3);
        EXPECT_CALL(observer, OnServerOffline(5u));
        ForwardTime(std::chrono::seconds(3));
    }

    TEST_F(CanProtocolClientLivenessTest, FrameBeforeTimeoutDefersOffline)
    {
        ExpectHeartbeats(5);
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

        ExpectHeartbeats(3);
        EXPECT_CALL(observer, OnServerOffline(5u));
        ForwardTime(std::chrono::seconds(3));

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateServerFrame(5);
    }

    TEST_F(CanProtocolClientLivenessTest, AllSlotsFull_EvictsOldestRoundRobin)
    {
        ExpectHeartbeats(3);
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
        client.ReserveSequence(5, 3, 0x10);

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateCommandAck(5, 3, 0x10, CanAckStatus::sequenceError, 7);

        EXPECT_EQ(client.ReserveSequence(5, 3, 0x10), 7u);
    }

    TEST_F(CanProtocolClientLivenessTest, SuccessAck_DoesNotResync)
    {
        client.ReserveSequence(5, 3, 0x10);

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateCommandAck(5, 3, 0x10, CanAckStatus::success, 0);

        EXPECT_EQ(client.ReserveSequence(5, 3, 0x10), 1u);
    }

    TEST_F(CanProtocolClientLivenessTest, CommandAckTimeout_FiresWhenNoAckArrives)
    {
        client.ReserveSequence(5, 3, 0x10);

        ExpectHeartbeats(1);
        EXPECT_CALL(observer, OnCommandAckTimeout(5u, 3u, 0x10u));
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(CanProtocolClientLivenessTest, CommandAckTimeout_DoesNotFireWhenAckArrives)
    {
        client.ReserveSequence(5, 3, 0x10);

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateCommandAck(5, 3, 0x10, CanAckStatus::success, 0);

        ExpectHeartbeats(2);
        ForwardTime(std::chrono::seconds(2));
    }

    TEST_F(CanProtocolClientLivenessTest, MismatchedAck_DoesNotClearAwaitingTimer)
    {
        client.ReserveSequence(5, 3, 0x10);

        EXPECT_CALL(observer, OnServerOnline(5u));
        SimulateCommandAck(5, 4, 0x20, CanAckStatus::success, 0);

        ExpectHeartbeats(1);
        EXPECT_CALL(observer, OnCommandAckTimeout(5u, 3u, 0x10u));
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(CanProtocolClientLivenessTest, CancelReservation_KeepsWaitingForTheEarlierCommandUntilItsOwnDeadline)
    {
        client.ReserveSequence(5, 3, 0x10);
        ForwardTime(std::chrono::milliseconds(500));

        EXPECT_EQ(client.ReserveSequence(5, 4, 0x20), 1u);
        client.CancelReservation(5, 1);
        ForwardTime(std::chrono::milliseconds(499));

        ExpectHeartbeats(1);
        EXPECT_CALL(observer, OnCommandAckTimeout(5u, 3u, 0x10u));
        ForwardTime(std::chrono::milliseconds(1));
    }

    TEST_F(CanProtocolClientLivenessTest, CancelReservation_OfTheOnlyCommandLeavesNothingAwaited)
    {
        client.ReserveSequence(5, 3, 0x10);
        client.CancelReservation(5, 0);

        ExpectHeartbeats(2);
        ForwardTime(std::chrono::seconds(2));
    }

    TEST_F(CanProtocolClientLivenessTest, ReturnSequence_StillReportsTheLostCommandThroughTheAckTimeout)
    {
        client.ReserveSequence(5, 3, 0x10);
        client.ReturnSequence(5, 0);

        ExpectHeartbeats(1);
        EXPECT_CALL(observer, OnCommandAckTimeout(5u, 3u, 0x10u));
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(CanProtocolClientLivenessTest, AckDeliveredWhileTheCommandIsBeingSent_IsMatchedToIt)
    {
        CommandingCategory category{ client.Transport(), client };

        auto answerWhileSending = [this](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& onDone)
        {
            SimulateCommandAck(5, 3, 0x10, CanAckStatus::success, 0);
            onDone(true);
        };

        EXPECT_CALL(observer, OnServerOnline(5u));
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(answerWhileSending).RetiresOnSaturation();
        ExpectHeartbeats(2);

        EXPECT_TRUE(category.Send(5));
        ForwardTime(std::chrono::seconds(2));
    }

    TEST_F(CanProtocolClientLivenessTest, SequenceErrorDeliveredWhileTheCommandIsBeingSent_ResynchronisesTheNextCommand)
    {
        CommandingCategory category{ client.Transport(), client };

        auto answerWhileSending = [this](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& onDone)
        {
            SimulateCommandAck(5, 3, 0x10, CanAckStatus::sequenceError, 42);
            onDone(true);
        };

        EXPECT_CALL(observer, OnServerOnline(5u));
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(answerWhileSending).RetiresOnSaturation();

        EXPECT_TRUE(category.Send(5));
        EXPECT_EQ(client.ReserveSequence(5, 3, 0x10), 42u);
    }

    TEST_F(CanProtocolClientLivenessTest, SendsHeartbeatAfterQuietPeriod)
    {
        ExpectHeartbeats(1);
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_ProcessFrameInterceptsMessage)
    {
        StrictMock<IsoTpTransportMock> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_));
        client.AttachIsoTpTransport(mockIsoTp);

        auto id = hal::Can::Id::Create29BitId(MakeCanId(CanPriority::command, 0x01, 0x01, 0));
        EXPECT_CALL(mockIsoTp, ProcessFrame(_, _)).WillOnce(Return(true));

        SimulateRx(id, MakeMessage({ 0x01 }));
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_LeavesTransferFailuresToTheApplication)
    {
        StrictMock<IsoTpTransportMock> mockIsoTp;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).Times(2);

        client.AttachIsoTpTransport(mockIsoTp);
        client.DetachIsoTpTransport();
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_ProcessFrameReturnsFalse_ContinuesNormalDispatch)
    {
        StrictMock<IsoTpTransportMock> mockIsoTp;
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

            bool Handle(const hal::Can::Message&) override
            {
                return true;
            }

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

        StrictMock<IsoTpTransportMock> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        client.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x05, 0x42, 0);
        uint8_t pduData[] = { 0xDE, 0xAD };
        capturedPduCallback(rawId, infra::MakeRange(pduData));

        EXPECT_TRUE(pduCategory.msg.pduReceived);
        client.UnregisterCategory(pduCategory);
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_DispatchPdu_NonzeroSourceNodeMarksServerAlive)
    {
        StrictMock<IsoTpTransportMock> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        client.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::response, canSystemCategoryId, canCategoryListResponseMessageTypeId, 7);
        uint8_t pduData[] = { 0x00 };
        capturedPduCallback(rawId, infra::MakeRange(pduData));
    }

    TEST_F(CanProtocolClientTest, AttachIsoTpTransport_DispatchPdu_UnknownCategory_Ignored)
    {
        StrictMock<IsoTpTransportMock> mockIsoTp;
        infra::Function<void(uint32_t, infra::ConstByteRange)> capturedPduCallback;
        EXPECT_CALL(mockIsoTp, SetOnPduReceived(_)).WillOnce(SaveArg<0>(&capturedPduCallback));
        client.AttachIsoTpTransport(mockIsoTp);

        uint32_t rawId = MakeCanId(CanPriority::command, 0x0F, 0x01, 0);
        uint8_t pduData[] = { 0xAA };
        capturedPduCallback(rawId, infra::MakeRange(pduData));
    }
}
