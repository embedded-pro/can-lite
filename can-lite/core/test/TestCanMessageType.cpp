#include "can-lite/core/CanMessageType.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace services;

    class MinimalMessageType : public CanMessageType
    {
    public:
        uint8_t Id() const override
        {
            return 0x01;
        }

        bool Handle(const hal::Can::Message&) override
        {
            return true;
        }
    };

    TEST(CanMessageTypeTest, DefaultHandlePduReturnsFalse)
    {
        MinimalMessageType msg;
        uint8_t data[] = { 0x01, 0x02 };
        EXPECT_FALSE(msg.HandlePdu(infra::MakeRange(data)));
    }
}
