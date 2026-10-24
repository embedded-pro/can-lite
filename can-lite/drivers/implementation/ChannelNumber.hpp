#pragma once

#include "infra/util/BoundedString.hpp"
#include "infra/util/Function.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>

namespace services
{
    inline std::optional<uint32_t> ParseChannelNumber(infra::BoundedConstString name)
    {
        if (name.empty() || name.size() > 4)
            return std::nullopt;

        uint32_t value{};
        for (const auto character : name)
        {
            if (character < '0' || character > '9')
                return std::nullopt;
            value = value * 10 + static_cast<uint32_t>(character - '0');
        }
        return value;
    }

    inline void ReportChannelNumber(uint32_t number, const infra::Function<void(infra::BoundedConstString)>& callback)
    {
        std::array<char, 12> text{};
        const auto length = std::snprintf(text.data(), text.size(), "%u", static_cast<unsigned>(number));
        if (length > 0)
            callback(infra::BoundedConstString(text.data(), static_cast<std::size_t>(length)));
    }
}
