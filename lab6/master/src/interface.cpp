#include "interface.hpp"

#include <algorithm>
#include <iostream>

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace utility;

namespace
{
    auto check_id(std::int64_t const id) noexcept(false) -> void
    {
        if (id < 0)
        {
            throw std::invalid_argument{"invalid node id (" + std::to_string(id) + ")"};
        }
    }

    auto check_parent_id(std::int64_t const parent_id) noexcept(false) -> void
    {
        if (parent_id < -1)
        {
            throw std::invalid_argument{"invalid parent id (" + std::to_string(parent_id) + ")"};
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

    [[nodiscard]]
    auto build_command_with_target(std::string_view const command, std::int64_t const target) noexcept(false)
    -> std::string
    {
        return std::string{command} + " " + std::to_string(target);
    }

    [[nodiscard]]
    auto build_command_with_special(std::string_view const command, std::string_view const special) noexcept(false)
    -> std::string
    {
        return std::string{command} + " " + std::string{special};
    }
}

auto executable::interface::declare() noexcept(false) -> void
{
    runner_.assign_or_update({
        .name = "create",
        .callback = [this](argv_t argv) -> network::response
        {
            commandline::argv::check(argv, std::array{"id"sv, "parent"sv});

            auto const target = std::stoll(std::string{argv[1]});
            auto const parent = std::stoll(std::string{argv[2]});

            check_id(target);
            check_parent_id(parent);

            //
            // Checkout target node for existing
            //
            if (auto response = engine_.exec(target, "ping");
                response.error == network::error::ok)
            {
                return {.error = network::error::exists};
            }
            else if (response.error != network::error::unknown)
            {
                return response;
            }

            auto const        port = pop_port_from_pool();
            network::response response;

            if (parent == -1)
            {
                //
                // Create node locally
                //
                response = engine_.create_node(target, port);
            }
            else
            {
                //
                // Create node remotely
                //
                auto const request = build_command_with_target("create", target);
                auto const primary = request + " " + std::to_string(port);
                response           = engine_.exec(parent, primary);
            }

            if (response.error != network::error::ok)
            {
                port_pool_.insert(port);
            }

            return response;
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
            commandline::argv::check(argv, std::array{"id"sv, "command"sv});

            auto const target  = std::stoll(std::string{argv[1]});
            auto const command = argv[2];

            check_id(target);
            check_special_command(command);

            auto const request_message = build_command_with_special("exec", command);
            return engine_.exec(target, request_message);
        }
    }).assign_or_update({
        .name = "ping",
        .callback = [this](argv_t argv) -> network::response
        {
            commandline::argv::check(argv, std::array{"id"sv});

            auto const target = std::stoll(std::string{argv[1]});

            check_id(target);

            auto response = engine_.exec(target, "ping");

            if (response.error == network::error::unavailable)
            {
                return
                {
                    .error = network::error::ok,
                    .message = "0",
                };
            }
            return response;
        }
    }).assign_or_update({
        .name = "/list",
        .callback = [this](argv_t argv) -> network::response
        {
            commandline::argv::check(argv, commandline::argv::no_args);

            std::cout <<
                "Common interface commands:\n"
                "    create [id:i64] [parent:i64]\n"
                "    remove [id:i64]\n"
                "    exec   [id:i64] [command:string]\n"
                "    ping   [id:i64]\n"
                "================================\n"
                "Additional commands:\n"
                "    /list : show list of available commands and their description\n"
                << std::flush;

            return {};
        }
    });
}
