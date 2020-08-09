#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>
#include <string_view>
#include <functional>
#include <stdexcept>

#include <utility/my_ranges.hpp>
#include <utility/string.hpp>

namespace utility::commandline
{
    using argv_t = std::vector<std::string_view> const&;

    template <typename Callable>
    class runner;

    template <typename R>
    class runner<R(std::vector<std::string_view> const&)>
    {
    public:
        using callback_t = std::function<R(argv_t)>;

        struct callback_info
        {
            std::string_view name;
            callback_t       callback;
        };

    private:
        using map_key_t = std::string_view;
        std::unordered_map<map_key_t, callback_t> callbacks_;

    public:
        /// Execute command string
        /**
         * @param command: command string
        */
        auto execute(std::string_view const command) noexcept(false) -> std::optional<R>
        {
            auto const argv = string::split_to_words(command);
            if (std::size(argv) == 0)
            {
                return std::nullopt;
            }

            auto const end = callbacks_.end();
            if (auto it = callbacks_.find(argv[0]); it != end)
            {
                auto& callback = it->second;
                return std::invoke(callback, argv);
            }

            throw std::invalid_argument{"no such command '" + std::string{argv[0]} + "'"};
        }

        /// Update information about command
        /**
         * @param name: command name
         * @param callback: new callback for the command
         * @return reference to the current object
        */
        auto assign_or_update(std::string_view const name, callback_t callback) & noexcept(false) -> runner&
        {
            callbacks_.insert_or_assign(name, std::move(callback));
            return *this;
        }

        /// Update information about command
        /**
         * @param name: command name
         * @param callback: new callback for the command
         * @return reference to the current object
        */
        auto assign_or_update(std::string_view const name, callback_t callback) && noexcept(false) -> runner&&
        {
            return std::move(assign_or_update(name, std::move(callback)));
        }

        /// Update information about command
        /**
         * @param info: information about command name and it's callback
         * @return reference to the current object
        */
        auto assign_or_update(callback_info info) & noexcept(false) -> runner&
        {
            return assign_or_update(info.name, std::move(info.callback));
        }

        /// Update information about command
        /**
         * @param info: information about command name and it's callback
         * @return reference to the current object
        */
        auto assign_or_update(callback_info info) && noexcept(false) -> runner&&
        {
            return std::move(assign_or_update(info.name, std::move(info.callback)));
        }
    };

    namespace argv
    {
        template <std::size_t Count>
        static auto check(std::vector<std::string_view> const&       argv,
                          std::array<std::string_view, Count> const& hint) noexcept(false)
        -> void
        {
            if (std::size(argv | my_ranges::views::drop(1)) != std::size(hint))
            {
                auto const prefix  = "incorrect number of arguments, '" + std::string{argv[0]} + "' takes";
                auto       message = prefix;

                if constexpr (/*std::size(hint)*/ Count != 0)
                {
                    // constexpr context via std::size(...) is unavailable
                    for (auto param : hint)
                    {
                        message += " [" + std::string{param} + "]";
                    }
                }
                else
                {
                    message += " nothing";
                }

                throw std::invalid_argument{message};
            }
        }

        auto constexpr no_args = std::array<std::string_view, 0>{};
    }
}
