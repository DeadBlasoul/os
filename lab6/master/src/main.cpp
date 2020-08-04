#include <iostream>
#include <iomanip>
#include <string>
#include <array>

#include <zmq.hpp>

#include <tasking/launcher.hpp>
#include <utility/commandline.hpp>
#include <utility/unrolled.hpp>
#include <network/graph.hpp>

namespace {
    namespace my_ranges::views {
        struct dropper {
            std::size_t count;
        };

        [[nodiscard]]
        auto drop(std::size_t const count) -> dropper {
            return {count};
        }

        template <
            typename T, typename... Other,
            template<typename, typename...> typename Collection>
        auto operator|(Collection<T, Other...> const& collection, dropper dropper) -> std::basic_string_view<T> {
            auto const size       = std::size(collection);
            auto const drop_count = std::min(size, dropper.count);
            return {std::data(collection) + drop_count, size - drop_count};
        }
    }
}

auto constexpr no_args = std::array<std::string_view, 0> {};

template <std::size_t Count>
auto check_argv(std::vector<std::string_view> const&       argv,
                std::array<std::string_view, Count> const& hint) noexcept(false)
-> void {
    if (std::size(argv | my_ranges::views::drop(1)) != std::size(hint)) {
        auto const prefix  = "incorrect number of arguments, '" + std::string {argv[0]} + "' takes";
        auto       message = prefix;

        if constexpr (/*std::size(hint)*/ Count != 0) {
            // constexpr context via std::size(...) is unavailable
            for (auto param : hint) {
                message += " [" + std::string {param} + "]";
            }
        }
        else {
            message += " nothing";
        }

        throw std::invalid_argument {message};
    }
}

auto main() -> int try {
    using namespace std::string_view_literals;
    using namespace utility;

    /// Reply reader
    auto static process_reply = [](network::response const& response) {
        switch (response.error) {
        case network::response::error_type::bad_request:
            throw std::runtime_error {"Bad request"};
        case network::response::error_type::unavailable:
            throw std::runtime_error {"Node is unavailable"};
        case network::response::error_type::unknown:
            throw std::invalid_argument {"Not found"};
        case network::response::error_type::exists:
            throw std::invalid_argument {"Already exists"};

        case network::response::error_type::ok:
            std::cout << "Ok: " << response.message << std::endl;
        }
    };

    auto context = zmq::context_t {1};
    auto net_graph = network::graph {context};
    auto runner  =
        //
        // CLI definition
        //
        commandline::command_runner {}
        .assign_or_update({
            /// Prints every argument passed in callback
            .name = "/test-arg",
            .callback = [](commandline::command_runner::argv_t const& argv) {
                auto print_entry = []() { std::cout << '['; };
                auto print_exit  = []() { std::cout << ']'; };
                auto print_body  = [&argv]() {
                    std::cout << argv[0];
                    for (auto const arg : argv | my_ranges::views::drop(1)) {
                        std::cout << ", " << arg;
                    }
                };

                print_entry();
                print_body();
                print_exit();

                std::cout << std::endl;
            }
        })
        .assign_or_update({
            .name = "/list",
            .callback = [&](commandline::command_runner::argv_t const& argv) {
                check_argv(argv, no_args);

                std::cout <<
                    "Common interface commands:\n"
                    "    create [id:i64] [parent:i64]\n"
                    "    remove [id:i64]\n"
                    "    exec   [id:i64] [command:string]\n"
                    "    ping   [id:i64]\n"
                    "================================\n"
                    "Additional commands:\n"
                    "    /test-arg <[]...> : test arguments parsing\n"
                    "    /list             : show list of available commands and their description\n"
                    << std::flush;
            }
        })
        .assign_or_update({
            /// Creates new computing node
            /**
             *  Format: 'create [id:i64] [parent:i64]'
             */
            .name = "create",
            .callback = [&](commandline::command_runner::argv_t const& argv) {
                check_argv(argv, std::array {"id:i64"sv, "parent:i64"sv});

                auto const id     = unrolled(argv[1]);
                auto const parent = unrolled(argv[2]);

                auto const rep = net_graph.create(id, parent);
                process_reply(rep);
            }
        })
        .assign_or_update({
            /// Removes existing computing node
            /**
             * Format: 'remove [id:i64]'
             */
            .name = "remove",
            .callback = [&](commandline::command_runner::argv_t const& argv) {
                check_argv(argv, std::array {"id:i64"sv});

                auto const id = unrolled(argv[1]);

                auto const rep = net_graph.remove(id);
                process_reply(rep);
            }
        })
        .assign_or_update({
            /// Executes command on the node
            /**
             * Format: 'exec [id:i64] [command:string]'
             */
            .name = "exec",
            .callback = [&](commandline::command_runner::argv_t const& argv) {
                check_argv(argv, std::array {"id:i64"sv, "command:string"sv});

                auto const id      = unrolled(argv[1]);
                auto const command = argv[2];

                auto const rep = net_graph.exec(id, command);
                process_reply(rep);
            }
        })
        .assign_or_update({
            /// Pings the node
            /**
             * Format: 'ping [id:i64]'
             */
            .name = "ping",
            .callback = [&](commandline::command_runner::argv_t const& argv) {
                check_argv(argv, std::array {"id:i64"sv});

                auto const id = unrolled(argv[1]);

                auto const rep = net_graph.ping(id);
                process_reply(rep);
            }
        });

    //
    // Wrapper around of passing the line to the runner
    //
    auto exec_command = [&runner](std::string_view const line) {
        try {
            runner.exec(line);
        }
        catch (std::exception const& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    };

    //
    // Help on startup
    //
    runner.exec("/list");

    //
    // Main loop
    //
    std::string line;
    while (std::cin) {
        std::cout << "\n~> ";
        std::getline(std::cin, line);
        exec_command(line);
    }
}
catch (std::exception& e) {
    std::cerr << "PANIC: " << e.what() << std::endl;
}
catch (...) {
    std::cerr << "PANIC: " << "unknown error" << std::endl;
}
