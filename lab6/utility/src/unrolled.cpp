#include <utility/unrolled.hpp>

#include <stdexcept>

std::int64_t utility::unrolled(std::string_view value) {
    auto result = std::int64_t{ 0 };
    auto negative = false;
    auto size = std::size(value);

    if (std::size(value) == 0) {
        throw std::invalid_argument{ "Empty string" };
    }
    if (value[0] == '-') {
        negative = true;
        value = value.substr(1, size - 1);
        --size;
    }
    for (auto const letter : value) {
        if (letter < '0' || '9' < letter) {
            throw std::invalid_argument{ "'" + std::string {value} + "' is not a number" };
        }
    }
    if (size > 19) {
        throw std::invalid_argument{ "Integer overflow" };
    }

    auto const length = value.size();
    switch (length) {
    default:
    case 0x13: result += (value[length - 0x13] - '0') * 1000000000000000000LL;
    case 0x12: result += (value[length - 0x12] - '0') * 100000000000000000LL;
    case 0x11: result += (value[length - 0x11] - '0') * 10000000000000000LL;
    case 0x10: result += (value[length - 0x10] - '0') * 1000000000000000LL;
    case 0x0f: result += (value[length - 0x0f] - '0') * 100000000000000LL;
    case 0x0e: result += (value[length - 0x0e] - '0') * 10000000000000LL;
    case 0x0d: result += (value[length - 0x0d] - '0') * 1000000000000LL;
    case 0x0c: result += (value[length - 0x0c] - '0') * 100000000000LL;
    case 0x0b: result += (value[length - 0x0b] - '0') * 10000000000LL;
    case 0x0a: result += (value[length - 0x0a] - '0') * 1000000000LL;
    case 0x09: result += (value[length - 0x09] - '0') * 100000000LL;
    case 0x08: result += (value[length - 0x08] - '0') * 10000000LL;
    case 0x07: result += (value[length - 0x07] - '0') * 1000000LL;
    case 0x06: result += (value[length - 0x06] - '0') * 100000LL;
    case 0x05: result += (value[length - 0x05] - '0') * 10000LL;
    case 0x04: result += (value[length - 0x04] - '0') * 1000LL;
    case 0x03: result += (value[length - 0x03] - '0') * 100LL;
    case 0x02: result += (value[length - 0x02] - '0') * 10LL;
    case 0x01: result += (value[length - 0x01] - '0');
    }

    return negative ? -result : result;
}