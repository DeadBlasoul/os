#include <iostream>
#include <iomanip>
#include <string>

#include <zmq.hpp>

#include <network/topology.hpp>
#include <utility/commandline.hpp>

#include "interface.hpp"

auto main() -> int try
{
    using namespace std::string_view_literals;
    using namespace utility;

    auto context   = zmq::context_t{1};
    auto engine    = network::topology::tree::engine{context};
    auto interface = executable::interface{engine};

    auto static process_reply = [](network::response const& response) -> void
    {
        switch (response.code)
        {
        case network::response::error::ok:
            if (response.message.empty())
            {
                std::cout << "Ok" << std::endl;
            }
            else
            {
                std::cout << "Ok: " << response.message << std::endl;
            }
            break;

        case network::response::error::bad_request:
            throw std::runtime_error{"Bad request"};

        case network::response::error::exists:
            throw std::logic_error{"Already exists"};

        case network::response::error::unknown:
            throw std::invalid_argument{"Node not found"};

        case network::response::error::unavailable:
            throw std::runtime_error{"Node is unavailable"};

        case network::response::error::invalid_path:
            throw std::runtime_error{
                "Some of branches is not operational for this request that makes result ambiguous"
            };

        case network::response::error::internal_error:
            if (response.message.empty())
            {
                throw std::runtime_error{"Internal error"};
            }
            throw std::runtime_error{"Internal error: " + std::string{response.message}};

        default:
            throw std::runtime_error{"Unknown error"};
        }
    };

    //
    // Wrapper around of passing the line to the runner
    //
    auto execute_command = [&interface](std::string_view const command)
    {
        try
        {
            auto const response = interface.execute(command);
            process_reply(response);
        }
        catch (std::exception const& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    };

    //
    // Help on startup
    //
    interface.execute("/list");

    //
    // Main loop
    //
    std::string line;
    while (std::cin)
    {
        std::cout << "\n~> ";
        std::getline(std::cin, line);
        execute_command(line);
    }
}
catch (std::exception& e)
{
    std::cerr << "PANIC: " << e.what() << std::endl;
}
catch (...)
{
    std::cerr << "PANIC: " << "unknown code" << std::endl;
}
