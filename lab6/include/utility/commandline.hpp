#pragma once

#include <iostream>
#include <map>
#include <unordered_map>
#include <memory>
#include <vector>
#include <string_view>
#include <functional>

namespace utility::commandline {
    class command_runner {
    public:
        using argv_t = std::vector<std::string_view>;
        using callback_t = std::function<void(argv_t const&)>;

        struct callback_info {
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
        auto exec(std::string_view command) noexcept(false) -> void;

        /// Update information about command
        /**
         * @param name: command name
         * @param callback: new callback for the command
         * @return reference to the current object
        */
        auto assign_or_update(std::string_view const name, callback_t callback) & noexcept(false) -> command_runner& {
            callbacks_.insert_or_assign(name, std::move(callback));
            return *this;
        }

        /// Update information about command
        /**
         * @param name: command name
         * @param callback: new callback for the command
         * @return reference to the current object
        */
        auto assign_or_update(std::string_view const name, callback_t callback) && noexcept(false) -> command_runner&& {
            return std::move(assign_or_update(name, std::move(callback)));
        }

        /// Update information about command
        /**
         * @param info: information about command name and it's callback
         * @return reference to the current object
        */
        auto assign_or_update(callback_info info) & noexcept(false) -> command_runner& {
            return assign_or_update(info.name, std::move(info.callback));
        }

        /// Update information about command
        /**
         * @param info: information about command name and it's callback
         * @return reference to the current object
        */
        auto assign_or_update(callback_info info) && noexcept(false) -> command_runner&& {
            return std::move(assign_or_update(info.name, std::move(info.callback)));
        }
    };
}
