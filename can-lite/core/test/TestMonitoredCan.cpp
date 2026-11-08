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

TEST_F(CanBusErrorCountersTest, errors_print_by_name)
{
    infra::StringOutputStream::WithStorage<64> stream;

    stream << CanBusError::busOff << ", " << CanBusError::rxBufferOverflow << ", " << static_cast<CanBusError>(99);

    EXPECT_EQ("bus off, rx buffer overflow, unknown", stream.Storage());
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
