#pragma once

#include "can-lite/drivers/interface/CanBusAdapter.hpp"
#include "infra/util/BoundedString.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    constexpr std::size_t slcanMaxCommandLength{ 32 };
    using SlcanCommand = infra::BoundedString::WithStorage<slcanMaxCommandLength>;

    std::optional<char> SlcanBitrateCode(uint32_t bitrate);
    SlcanCommand SlcanOpenCommands(char bitrateCode);
    SlcanCommand SlcanCloseCommand();
    SlcanCommand EncodeSlcanFrame(uint32_t id, bool extended, const CanFrame& data);

    struct SlcanFrame
    {
        uint32_t id{};
        bool extended{};
        bool remote{};
        CanFrame data;
    };

    class SlcanDecoder
    {
    public:
        static constexpr std::size_t maxLineLength{ 64 };

        enum class Event
        {
            frame,
            acknowledged,
            error
        };

        struct Item
        {
            Event event{ Event::acknowledged };
            SlcanFrame frame;
        };

        std::optional<Item> Feed(char byte);
        void Reset();

    private:
        static std::optional<Item> ParseLine(infra::BoundedConstString line);

        infra::BoundedString::WithStorage<maxLineLength> line;
        bool discarding{};
    };
}
