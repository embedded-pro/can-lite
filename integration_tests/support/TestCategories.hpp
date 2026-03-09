#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageType.hpp"

namespace integration
{
    class TestMessageType : public services::CanMessageType
    {
    public:
        explicit TestMessageType(uint8_t id);

        uint8_t Id() const override;
        void Handle(const hal::Can::Message&) override;

        int handleCount = 0;

    private:
        uint8_t msgId;
    };

    class SequencedTestCategory : public services::CanCategoryServer
    {
    public:
        explicit SequencedTestCategory(uint8_t id);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

        TestMessageType msg;

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
