#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageType.hpp"
#include <cstddef>

namespace integration
{
    class TestMessageType : public services::CanMessageType
    {
    public:
        TestMessageType(uint8_t id, std::size_t minimumPayloadSize);

        uint8_t Id() const override;
        void Handle(const hal::Can::Message&) override;

        int handleCount = 0;
        int rejectedCount = 0;

    private:
        uint8_t msgId;
        std::size_t minimumPayloadSize;
    };

    class SequencedTestCategory : public services::CanCategoryServer
    {
    public:
        explicit SequencedTestCategory(uint8_t id);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

        TestMessageType msg;
        TestMessageType validatedMsg;

    private:
        uint8_t catId;
    };

    class SimpleTestCategory : public services::CanCategoryServer
    {
    public:
        explicit SimpleTestCategory(uint8_t id);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

    private:
        uint8_t catId;
    };
}
