#pragma once

#include "can-lite/core/CanCategory.hpp"
#include <cstddef>
#include <cstdint>

namespace integration
{
    class SequencedTestCategory
        : private services::CanCategoryHandlerStorage<2>
        , public services::CanCategoryServer
    {
    public:
        explicit SequencedTestCategory(uint8_t id);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

        int handleCount = 0;
        int rejectedCount = 0;
        int validatedHandleCount = 0;
        int validatedRejectedCount = 0;

        static constexpr uint8_t messageTypeId = 0x01;
        static constexpr uint8_t validatedMessageTypeId = 0x02;
        static constexpr std::size_t validatedMinimumPayloadSize = 4;

    private:
        uint8_t catId;
    };

    class SimpleTestCategory
        : private services::CanCategoryHandlerStorage<1>
        , public services::CanCategoryServer
    {
    public:
        explicit SimpleTestCategory(uint8_t id);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

    private:
        uint8_t catId;
    };
}
