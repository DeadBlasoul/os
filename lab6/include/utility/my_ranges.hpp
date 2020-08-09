#pragma once

#include <cstddef>
#include <string_view>

#undef min

namespace utility::my_ranges::views
{
    struct dropper
    {
        std::size_t count;
    };

    [[nodiscard]]
    auto inline drop(std::size_t const count) -> dropper
    {
        return {count};
    }

    template <
        typename T, typename... Other,
        template<typename, typename...> typename Collection>
    auto operator|(Collection<T, Other...> const& collection, dropper dropper) -> std::basic_string_view<T>
    {
        auto const size       = std::size(collection);
        auto const drop_count = std::min(size, dropper.count);
        return {std::data(collection) + drop_count, size - drop_count};
    }
}
