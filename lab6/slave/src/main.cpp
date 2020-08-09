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

#include <utility/commandline.hpp>
#include <network/response.hpp>

auto main(int const argc, char const* argv[]) -> int try
{
    using namespace utility;

    if (argc != 3)
    {
        throw std::invalid_argument{"Incorrect number of arguments"};
    }

    std::cout
        << "[#] New node created with the following parameters:" << std::endl
        << "    path    : " << argv[0] << std::endl
        << "    address : " << argv[1] << std::endl
        << "    id      : " << argv[2] << std::endl;


    //
    //  As we start program via CreateProcess it's doesn't receive argv[0] as path to program which started.
    //  Following it argv[0] will be an address, argv[1] will be an unique id. 
    //
    auto const* address = argv[1];
    auto const  id      = std::stoll(argv[2]);

    //
    //  Prepare our context and socket
    //
    auto context = zmq::context_t{1};
    auto socket  = zmq::socket_t{context, ZMQ_REP};
    socket.bind(address);

    while (true)
    {
        //
        // Receive request
        //
        auto request  = zmq::message_t{256};
        auto recv_res = socket.recv(request, zmq::recv_flags::none);
        request.rebuild(*recv_res);

        std::cout << std::endl;
        std::cout << "[^] Request : " << std::string_view{request.data<char>(), request.size()} << std::endl;

        //
        // Build response
        //
        auto const process_id = GetCurrentProcessId();
        auto const response   = network::response{
            .code = network::response::error::ok,
            .message = std::to_string(process_id),
        };

        //
        // Send response
        //
        auto serialized_response = zmq::message_t{response.size()};
        response.serialize_to(serialized_response);
        socket.send(serialized_response, zmq::send_flags::none);

        std::cout << "[v] Reply   : " << response.message << std::endl;
    }
}
catch (std::exception& e)
{
    std::cerr << "Error: " << e.what() << std::endl;
#undef max
    zmq_sleep(std::numeric_limits<int>::max());
}
