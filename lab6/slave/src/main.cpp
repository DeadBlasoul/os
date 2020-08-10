//
//  Hello World client in C++
//  Connects REQ socket to tcp://localhost:5555
//  Sends "Hello" to server, expects "World" back
//
#include <zmq.hpp>
#include <string>
#include <iostream>
#include <sstream>

#include <Windows.h>

#undef interface

#include <utility/commandline.hpp>
#include <network/response.hpp>
#include <network/topology.hpp>

#include "interface.hpp"

auto main(int const argc, char const* argv[]) -> int try
{
    using namespace utility;

    if (argc != 3)
    {
        throw std::invalid_argument{"Incorrect number of arguments"};
    }

    std::cout
        << "[#] New node created with the following parameters:" << std::endl
        << "    path     : " << argv[0] << std::endl
        << "    address  : " << argv[1] << std::endl
        << "    id       : " << argv[2] << std::endl;

    //
    //  As we start program via CreateProcess it's doesn't receive argv[0] as path to program which started.
    //  Following it argv[0] will be an address, argv[1] will be an unique id. 
    //
    auto const* address = argv[1];
    auto const  id      = std::stoll(argv[2]);

    //
    //  Prepare our context and socket
    //
    auto context   = zmq::context_t{1};
    auto engine    = network::topology::tree::engine{context};
    auto interface = executable::interface{engine, id};
    auto socket    = zmq::socket_t{context, ZMQ_REP};
    socket.bind(address);
    auto send_response = [&socket](network::response const& response) -> void
    {
        auto serialized_response = zmq::message_t{response.size()};
        response.serialize_to(serialized_response);
        socket.send(serialized_response, zmq::send_flags::dontwait);

        std::cout << "[v] Response : [" << response.code_to_string() << "] " << response.message << std::endl;
    };

    while (not interface.kill_requested())
    {
        //
        // Receive request
        //
        auto       serialized_request = zmq::message_t{256};
        auto const result             = socket.recv(serialized_request, zmq::recv_flags::none);
        serialized_request.rebuild(*result);

        //
        // Process request
        //
        try
        {
            auto request = network::request{};
            request.deserialize_from(serialized_request);

            std::cout << std::endl;
            std::cout << "[^] Request  : [" << request.code_to_string() << "] " << request.message << std::endl;

            if (request.type == network::request::type::envelope)
            {
                //
                // Send 'ok' back immediately on envelope request
                //
                send_response({.error = network::error::ok});
                continue;
            }

            auto request_stream = std::stringstream{request.message};
            auto target_id      = std::int64_t{};
            if (not (request_stream >> target_id))
            {
                throw std::invalid_argument{"invalid target id"};
            }

            auto response = network::response{};

            if (target_id == id || target_id == network::topology::any_node)
            {
                auto command = std::string{};

                for (auto line = std::string{}; std::getline(request_stream, line);)
                {
                    command += line + "\n";
                }

                response = interface.execute(command);
            }
            else
            {
                response = engine.ask_every_until_response(target_id, request.message);
            }

            //
            // Send response
            //
            send_response(response);
        }
        catch (std::runtime_error const& e)
        {
            send_response({
                .error = network::error::internal_error,
                .message = e.what(),
            });
        }
        catch (std::invalid_argument const& e)
        {
            send_response({
                .error = network::error::bad_request,
                .message = e.what(),
            });
        }
        catch (std::exception const& e)
        {
            send_response({
                .error = network::error::internal_error,
                .message = e.what(),
            });
        }
    }
}
catch (std::exception& e)
{
    std::cerr << "PANIC: " << e.what() << std::endl;
#undef max
    zmq_sleep(std::numeric_limits<int>::max());
}
