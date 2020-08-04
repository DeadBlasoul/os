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

auto main(int const argc, char const* argv[]) -> int try {
    using namespace utility;

    if (argc != 3) {
        throw std::invalid_argument {"Incorrect number of arguments"};
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
    auto context = zmq::context_t {1};
    auto socket  = zmq::socket_t {context, ZMQ_REP};
    socket.bind(address);

    auto runner =
        commandline::command_runner {}
        .assign_or_update({
            .name = "create",
            .callback = [&socket](commandline::command_runner::argv_t const& argv) {
                void(0);
            },
        })
        .assign_or_update({
            .name = "remove",
            .callback = [&socket](commandline::command_runner::argv_t const& argv) {
                void(0);
            },
        })
        .assign_or_update({
            .name = "exec",
            .callback = [&socket](commandline::command_runner::argv_t const& argv) {
                void(0);
            },
        })
        .assign_or_update({
            .name = "ping",
            .callback = [&socket](commandline::command_runner::argv_t const& argv) {
                void(0);
            },
        })
        .assign_or_update({
            .name = "pid",
            .callback = [&socket](commandline::command_runner::argv_t const& argv) {
                void(0);
            },
        });

    while (true) {
        auto message  = zmq::message_t {256};
        auto recv_res = socket.recv(message, zmq::recv_flags::none);
        message.rebuild(*recv_res);

        std::cout << std::endl;
        std::cout << "[^] Request : " << std::string_view {message.data<char>(), message.size()} << std::endl;

        auto const process_id   = GetCurrentProcessId();
        auto const reply_string = (std::stringstream {} << "OK " << process_id).str();

        auto reply = zmq::message_t {reply_string.size()};
        std::memcpy(reply.data(), reply_string.data(), reply_string.size());
        socket.send(reply, zmq::send_flags::none);
        std::cout << "[v] Reply   : " << reply_string << std::endl;
    }
}
catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    #undef max
    zmq_sleep(std::numeric_limits<int>::max());
}
