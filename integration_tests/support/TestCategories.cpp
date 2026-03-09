#include "support/TestCategories.hpp"

namespace integration
{
    TestMessageType::TestMessageType(uint8_t id)
        : msgId(id)
    {}

    uint8_t TestMessageType::Id() const
    {
        return msgId;
    }

    void TestMessageType::Handle(const hal::Can::Message&)
    {
        handleCount++;
    }

    SequencedTestCategory::SequencedTestCategory(uint8_t id)
        : catId(id)
        , msg(0x01)
    {
        AddMessageType(msg);
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
