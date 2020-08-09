#pragma once

#include <cstdint>
#include <string>

namespace network
{
    struct response
    {
        enum class error : std::uint8_t
        {
            ok,
            bad_request,
            unknown,
            unavailable,
            exists,
            invalid_path,
            internal_error,
        };

        error       code;
        std::string message;

        [[nodiscard]]
        auto size() const noexcept -> std::size_t
        {
            return sizeof(code) + std::size(message);
        }

        template <typename Container>
        auto serialize_to(Container& buffer) const noexcept(false) -> void
        {
            auto const size = buffer.size();

            if (size < this->size())
            {
                throw std::invalid_argument{"not enough space in serialization buffer"};
            }

            static_assert(
                std::is_same_v<void*, decltype(buffer.data())>,
                "type of value from data method in serialization buffer is not void*");

            auto* const data         = buffer.data();
            auto* const code_byte    = static_cast<error*>(data);
            auto* const string_space = reinterpret_cast<std::string::value_type*>(
                static_cast<std::byte*>(data) +
                sizeof(error));

            *code_byte = code;
            std::memcpy(string_space, message.data(), message.size());
        }

        template <typename Container>
        auto deserialize_from(Container const& buffer) noexcept(false) -> void
        {
            auto const size = buffer.size();

            if (size < sizeof(error))
            {
                throw std::invalid_argument{"size of data in serialized buffer is too small"};
            }

            static_assert(
                std::is_same_v<const void*, decltype(buffer.data())>,
                "type of value from data method in serialization buffer is not const void*");

            auto const* const data         = buffer.data();
            auto const* const code_byte    = static_cast<const error*>(data);
            auto const* const string_space = reinterpret_cast<const std::string::value_type*>(
                static_cast<const std::byte*>(data) +
                sizeof(error));

            code    = *code_byte;
            message = std::string{string_space, size - sizeof(error)};
        }
    };
}
