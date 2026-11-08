#include "can-lite/core/MonitoredCan.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "gtest/gtest.h"
#include <optional>

namespace
{
    using services::CanBusError;

    class CanBusErrorCountersTest
        : public testing::Test
    {
    public:
        services::CanBusErrorCounters counters;
    };

    class MonitoredCanImplTest
        : public testing::Test
    {
    public:
        MonitoredCanImplTest()
        {
            can.SetOnError([this](CanBusError error)
                {
                    reported = error;
                });
        }

        services::MonitoredCanImpl<testing::StrictMock<hal::CanMock>> can;
        std::optional<CanBusError> reported;
    };

    class DriverForwardingTo
        : public hal::Can
    {
    public:
        explicit DriverForwardingTo(hal::Can& bus)
            : bus(bus)
        {}

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override
        {
            bus.SendData(id, data, actionOnCompletion);
        }

        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override
        {
            bus.ReceiveData(receivedAction);
        }

    private:
        hal::Can& bus;
    };

    class MonitoredCanImplForwardingTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::CanMock> bus;
        services::MonitoredCanImpl<DriverForwardingTo> can{ bus };
    };
}

TEST_F(CanBusErrorCountersTest, fresh_counters_are_empty)
{
    EXPECT_EQ(0u, counters.Total());
    EXPECT_EQ(0u, counters.Count(CanBusError::busOff));
}

TEST_F(CanBusErrorCountersTest, every_class_is_counted)
{
    counters.Record(CanBusError::busOff);
    counters.Record(CanBusError::stuffError);
    counters.Record(CanBusError::stuffError);
    counters.Record(CanBusError::rxBufferOverflow);

    EXPECT_EQ(1u, counters.Count(CanBusError::busOff));
    EXPECT_EQ(2u, counters.Count(CanBusError::stuffError));
    EXPECT_EQ(1u, counters.Count(CanBusError::rxBufferOverflow));
    EXPECT_EQ(0u, counters.Count(CanBusError::crcError));
    EXPECT_EQ(4u, counters.Total());
}

TEST_F(CanBusErrorCountersTest, a_counter_saturates_instead_of_wrapping)
{
    EXPECT_EQ(1u, services::CanBusErrorCounters::SaturatingIncrement(0));
    EXPECT_EQ(UINT32_MAX, services::CanBusErrorCounters::SaturatingIncrement(UINT32_MAX - 1));
    EXPECT_EQ(UINT32_MAX, services::CanBusErrorCounters::SaturatingIncrement(UINT32_MAX));
}

TEST_F(CanBusErrorCountersTest, an_unknown_class_is_counted_as_other)
{
    counters.Record(static_cast<CanBusError>(99));

    EXPECT_EQ(1u, counters.Count(CanBusError::other));
    EXPECT_EQ(1u, counters.Total());
}

TEST_F(CanBusErrorCountersTest, reset_clears_every_class)
{
    counters.Record(CanBusError::busOff);
    counters.Record(CanBusError::ackError);
    counters.Reset();

    EXPECT_EQ(0u, counters.Total());
    EXPECT_EQ(0u, counters.Count(CanBusError::busOff));
    EXPECT_EQ(0u, counters.Count(CanBusError::ackError));
}

TEST_F(CanBusErrorCountersTest, every_error_prints_by_name)
{
    infra::StringOutputStream::WithStorage<256> stream;

    stream << CanBusError::busOff << "," << CanBusError::errorPassive << "," << CanBusError::errorWarning << ","
           << CanBusError::messageLost << "," << CanBusError::rxBufferOverflow << "," << CanBusError::ackError << ","
           << CanBusError::stuffError << "," << CanBusError::formError << "," << CanBusError::crcError << ","
           << CanBusError::bit0Error << "," << CanBusError::bit1Error << "," << CanBusError::other << ","
           << static_cast<CanBusError>(99);

    EXPECT_EQ("bus off,error passive,error warning,message lost,rx buffer overflow,ack error,stuff error,"
              "form error,crc error,bit0 error,bit1 error,other,unknown",
        stream.Storage());
}

TEST_F(MonitoredCanImplTest, an_error_is_counted_and_forwarded)
{
    can.InvokeErrorHandler(CanBusError::busOff);

    EXPECT_EQ(1u, can.ErrorStatistics().Count(CanBusError::busOff));
    EXPECT_EQ(CanBusError::busOff, reported);
}

TEST_F(MonitoredCanImplTest, an_error_is_counted_without_a_handler)
{
    can.SetOnError(nullptr);

    can.InvokeErrorHandler(CanBusError::crcError);

    EXPECT_EQ(1u, can.ErrorStatistics().Count(CanBusError::crcError));
    EXPECT_FALSE(reported.has_value());
}

TEST_F(MonitoredCanImplTest, statistics_reset_through_the_bus)
{
    can.InvokeErrorHandler(CanBusError::formError);
    can.ResetErrorStatistics();

    EXPECT_EQ(0u, can.ErrorStatistics().Total());
}

TEST_F(MonitoredCanImplForwardingTest, transmissions_reach_the_driver)
{
    const auto id = hal::Can::Id::Create29BitId(0x123);
    EXPECT_CALL(bus, SendData(id, testing::_, testing::_));

    can.SendData(id, hal::Can::Message{}, [](bool) {});
}

TEST_F(MonitoredCanImplForwardingTest, reception_is_installed_on_the_driver)
{
    EXPECT_CALL(bus, ReceiveData(testing::_));

    can.ReceiveData([](hal::Can::Id, const hal::Can::Message&) {});
}
