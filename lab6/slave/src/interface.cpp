#include "interface.hpp"

#include <algorithm>
#include <iostream>

#include <Windows.h>

using namespace std::string_view_literals;
using namespace utility;

#undef interface

namespace
{
    auto check_id(std::int64_t const id) noexcept(false) -> void
    {
        if (id < 0)
        {
            throw std::invalid_argument{"invalid node id (" + std::to_string(id) + ")"};
        }
    }

    auto check_special_command(std::string_view const command) noexcept(false) -> void
    {
        auto static constexpr defined_commands = std::array{
            "start"sv,
            "stop"sv,
            "time"sv,
        };

        auto constexpr begin = defined_commands.begin();
        auto constexpr end   = defined_commands.end();

        if (not std::any_of(begin, end, [&](auto const defined) { return defined == command; }))
        {
            throw std::invalid_argument{"no such special command '" + std::string{command} + "'"};
        }
    }
}

auto executable::interface::declare() noexcept(false) -> void
{
    runner_.assign_or_update({
        .name = "create",
        .callback = [this](argv_t argv) -> network::response
        {
            commandline::argv::check(argv, std::array{"id"sv, "port"sv});

            auto const target = std::stoll(std::string{argv[1]});
            auto const port   = static_cast<std::uint16_t>(std::stoul(std::string{argv[1]}));
            check_id(target);

            if (engine_.size() > 0)
            {
                return network::response
                {
                    .error = network::error::bad_request,
                    .message = "Node already has a children",
                };
            }

            return engine_.create_node(target, port);
        }
    }).assign_or_update({
        .name = "remove",
        .callback = [this](argv_t argv) -> network::response
        {
            commandline::argv::check(argv, std::array{"id"sv});

            auto const target_id = std::stoll(std::string{argv[1]});
            check_id(target_id);

            return engine_.remove(target_id);
        }
    }).assign_or_update({
        .name = "exec",
        .callback = [this](argv_t argv) -> network::response
        {
            commandline::argv::check(argv, std::array{"command"sv});

            auto const command = argv[1];
            check_special_command(command);

            /// Utility function
            auto build_response_message = [this](std::string_view const message = "") -> std::string
            {
                if (not message.empty())
                {
                    return std::to_string(id_) + ": " + std::string{ message };
                }
                else
                {
                    return std::to_string(id_);
                }
            };

            //
            // Timer data
            //
            auto static timestamp  = std::chrono::high_resolution_clock::now();
            auto static last_point = timestamp;
            auto static stopped = true;

            //
            // Start command
            //
            if (command == "start")
            {
                stopped = false;
                timestamp = std::chrono::high_resolution_clock::now();

                return
                {
                    .error = network::error::ok,
                    .message = build_response_message()
                };
            }

            //
            // Stop command
            //
            if (command == "stop")
            {
                if (not stopped)
                {
                    last_point = std::chrono::high_resolution_clock::now();
                }
                stopped = true;

                return
                {
                    .error = network::error::ok,
                    .message = build_response_message()
                };
            }

            //
            // Time command
            //
            if (command == "time")
            {
                if (not stopped)
                {
                    last_point = std::chrono::high_resolution_clock::now();
                }

                auto const time = (last_point - timestamp).count();
                return
                {
                    .error = network::error::ok,
                    .message = build_response_message(std::to_string(time / 1000000))
                };
            }

            throw std::runtime_error{ "Command '" + std::string{command} + "' not implemented" };
        }
    }).assign_or_update({
        .name = "ping",
        .callback = [this](argv_t argv) -> network::response
        {
            commandline::argv::check(argv, commandline::argv::no_args);

            return network::response
            {
                .error = network::error::ok,
                .message = "1",
            };
        }
    }).assign_or_update({
        .name = "pid",
        .callback = [this](argv_t argv) -> network::response
        {
            commandline::argv::check(argv, commandline::argv::no_args);

            auto const pid = GetCurrentProcessId();

            return network::response
            {
                .error = network::error::ok,
                .message = std::to_string(pid),
            };
        }
    }).assign_or_update({
        .name = "kill",
        .callback = [this](argv_t argv) -> network::response
        {
            commandline::argv::check(argv, commandline::argv::no_args);

            engine_.exec(network::topology::any_node, "kill");
            killed_ = true;

            std::cout << "I should be dead x_x" << std::endl;

            return {.error = network::error::ok};
        }
    });
}
