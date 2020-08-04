#pragma once

#include <cstdint>
#include <string_view>

namespace utility {
    auto unrolled(std::string_view value) noexcept(false) -> std::int64_t;
}
