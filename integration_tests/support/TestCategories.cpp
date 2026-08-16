#include "support/TestCategories.hpp"

namespace integration
{
    TestMessageType::TestMessageType(uint8_t id, std::size_t minimumPayloadSize)
        : msgId(id)
        , minimumPayloadSize(minimumPayloadSize)
    {}

    uint8_t TestMessageType::Id() const
    {
        return msgId;
    }

    void TestMessageType::Handle(const hal::Can::Message& data)
    {
        if (data.size() < minimumPayloadSize)
        {
            rejectedCount++;
            return;
        }

        handleCount++;
    }

    SequencedTestCategory::SequencedTestCategory(uint8_t id)
        : msg(0x01, 0)
        , validatedMsg(0x02, 4)
        , catId(id)
    {
        AddMessageType(msg);
        AddMessageType(validatedMsg);
    }

    uint8_t SequencedTestCategory::Id() const
    {
        return catId;
    }

    bool SequencedTestCategory::RequiresSequenceValidation() const
    {
        return true;
    }

    SimpleTestCategory::SimpleTestCategory(uint8_t id)
        : catId(id)
    {}

    uint8_t SimpleTestCategory::Id() const
    {
        return catId;
    }

    bool SimpleTestCategory::RequiresSequenceValidation() const
    {
        return false;
    }
}
