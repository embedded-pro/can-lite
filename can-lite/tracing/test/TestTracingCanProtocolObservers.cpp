#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/tracing/TracingCanProtocolClientObserver.hpp"
#include "can-lite/tracing/TracingCanProtocolServerObserver.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/tracer/Tracer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace testing;
    using namespace services;

    struct CanFixtureInit
    {
        CanFixtureInit(hal::CanMock& canMock,
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

    class CanProtocolServerObserverMock
        : public CanProtocolServerObserver
    {
    public:
        using CanProtocolServerObserver::CanProtocolServerObserver;

        MOCK_METHOD(void, Online, (), (override));
        MOCK_METHOD(void, Offline, (), (override));
    };

    class CanProtocolClientObserverMock
        : public CanProtocolClientObserver
    {
    public:
        using CanProtocolClientObserver::CanProtocolClientObserver;

        MOCK_METHOD(void, OnServerOnline, (uint16_t nodeId), (override));
        MOCK_METHOD(void, OnServerOffline, (uint16_t nodeId), (override));
        MOCK_METHOD(void, OnCommandAckTimeout, (uint16_t nodeId, uint8_t category, uint8_t messageType), (override));
    };

    class TracingCanProtocolServerObserverTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        void SimulateClientHeartbeat()
        {
            auto id = hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, 1));
            hal::Can::Message message;
            message.push_back(canProtocolVersion);
            receiveCallback(id, message);
        }

        infra::StringOutputStream::WithStorage<512> stream;
        TracerToStream tracer{ stream };
        CanProtocolServer::Config config{ 1, 500, std::chrono::seconds(1) };
        StrictMock<hal::CanMock> canMock;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        CanFixtureInit fixtureInit{ canMock, receiveCallback };
        CanProtocolServer server{ canMock, config };
        StrictMock<CanProtocolServerObserverMock> application{ server };
        TracingCanProtocolServerObserver tracing{ server, tracer };
    };

    TEST_F(TracingCanProtocolServerObserverTest, OnlineReachesBothObservers)
    {
        EXPECT_CALL(application, Online());

        SimulateClientHeartbeat();

        EXPECT_EQ("\r\nTracingCanProtocolServerObserver: Online", stream.Storage());
    }

    TEST_F(TracingCanProtocolServerObserverTest, OfflineReachesBothObservers)
    {
        EXPECT_CALL(application, Online());
        SimulateClientHeartbeat();

        EXPECT_CALL(application, Offline());
        ForwardTime(std::chrono::seconds(3));

        EXPECT_EQ("\r\nTracingCanProtocolServerObserver: Online"
                  "\r\nTracingCanProtocolServerObserver: Offline",
            stream.Storage());
    }

    class TracingCanProtocolClientObserverTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        void SimulateServerFrame(uint16_t sourceNodeId)
        {
            auto id = hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::heartbeat, canSystemCategoryId, canHeartbeatMessageTypeId, sourceNodeId));
            receiveCallback(id, hal::Can::Message{});
        }

        infra::StringOutputStream::WithStorage<512> stream;
        TracerToStream tracer{ stream };
        StrictMock<hal::CanMock> canMock;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        CanFixtureInit fixtureInit{ canMock, receiveCallback };
        CanProtocolClient::Config config{ std::chrono::seconds(3) };
        CanProtocolClient client{ canMock, config };
        StrictMock<CanProtocolClientObserverMock> application{ client };
        TracingCanProtocolClientObserver tracing{ client, tracer };
    };

    TEST_F(TracingCanProtocolClientObserverTest, ServerOnlineReachesBothObservers)
    {
        EXPECT_CALL(application, OnServerOnline(0x123u));

        SimulateServerFrame(0x123);

        EXPECT_EQ("\r\nTracingCanProtocolClientObserver: ServerOnline node 0x123", stream.Storage());
    }

    TEST_F(TracingCanProtocolClientObserverTest, ServerOfflineReachesBothObservers)
    {
        EXPECT_CALL(application, OnServerOnline(0x123u));
        SimulateServerFrame(0x123);

        EXPECT_CALL(application, OnServerOffline(0x123u));
        ForwardTime(std::chrono::seconds(3));

        EXPECT_EQ("\r\nTracingCanProtocolClientObserver: ServerOnline node 0x123"
                  "\r\nTracingCanProtocolClientObserver: ServerOffline node 0x123",
            stream.Storage());
    }

    TEST_F(TracingCanProtocolClientObserverTest, CommandAckTimeoutReachesBothObservers)
    {
        client.PeekSequence(5);
        client.CommitSequence(5, 3, 0x10);

        EXPECT_CALL(application, OnCommandAckTimeout(5u, 3u, 0x10u));
        ForwardTime(std::chrono::seconds(1));

        EXPECT_EQ("\r\nTracingCanProtocolClientObserver: CommandAckTimeout node 0x5 cat 0x3 type 0x10", stream.Storage());
    }
}
