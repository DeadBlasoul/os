#pragma once

#include <cstdint>
#include <string>

namespace network
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

    struct response
    {
        error       error;
        std::string message;

        [[nodiscard]]
        auto size() const noexcept -> std::size_t
        {
            return sizeof(error) + std::size(message);
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
            auto* const code_byte    = static_cast<network::error*>(data);
            auto* const string_space = reinterpret_cast<std::string::value_type*>(
                static_cast<std::byte*>(data) +
                sizeof(network::error));

            *code_byte = error;
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
            auto const* const code_byte    = static_cast<const network::error*>(data);
            auto const* const string_space = reinterpret_cast<const std::string::value_type*>(
                static_cast<const std::byte*>(data) +
                sizeof(network::error));

            error    = *code_byte;
            message = std::string{string_space, size - sizeof(error)};
        }

        [[nodiscard]]
        auto code_to_string() const noexcept -> std::string_view
        {
            switch (error)
            {
            case error::ok:
                return "ok";
            case error::bad_request:
                return "bad_request";
            case error::unavailable:
                return "unavailable";
            case error::exists:
                return "exists";
            case error::internal_error:
                return "internal_error";
            case error::invalid_path:
                return "invalid_path";
            case error::unknown:
                return "unknown";
            }

            return "INVALID_CODE";
        }
    };
}
