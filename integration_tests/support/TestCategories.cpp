#include "support/TestCategories.hpp"

namespace integration
{
    SequencedTestCategory::SequencedTestCategory(uint8_t id)
        : services::CanCategoryServer(messageTypeStorage)
        , catId(id)
    {
        AddMessageType(messageTypeId, [this](infra::ConstByteRange)
            {
                handleCount++;
                return true;
            });

        AddMessageType(validatedMessageTypeId, [this](infra::ConstByteRange payload)
            {
                if (payload.size() < validatedMinimumPayloadSize)
                {
                    validatedRejectedCount++;
                    return false;
                }

                validatedHandleCount++;
                return true;
            });
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
        : services::CanCategoryServer(messageTypeStorage)
        , catId(id)
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
