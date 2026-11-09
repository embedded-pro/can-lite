#pragma once

#include "can-lite/drivers/codec/SlcanCodec.hpp"
#include "infra/util/BoundedString.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    constexpr std::size_t virtualBusMaxNameLength{ 24 };
    using VirtualBusName = infra::BoundedString::WithStorage<virtualBusMaxNameLength>;

    SlcanCommand VirtualBusJoin(infra::BoundedConstString bus);

    class SlcanCommandDecoder
    {
    public:
        enum class Command
        {
            join,
            close,
            bitrate,
            open,
            frame,
            unknown
        };

        struct Item
        {
            Command command{ Command::unknown };
            uint32_t bitrate{};
            VirtualBusName name;
            SlcanFrame frame;
        };

        std::optional<Item> Feed(char byte);
        void Reset();

    private:
        static Item ParseLine(infra::BoundedConstString line);

        infra::BoundedString::WithStorage<SlcanDecoder::maxLineLength> line;
        bool discarding{};
    };
}
