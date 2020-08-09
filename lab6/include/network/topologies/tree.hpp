#pragma once

#include <set>
#include <string_view>

#include <zmq.hpp>

#include <tasking/launcher.hpp>
#include <network/response.hpp>

namespace network::topology::tree
{
    using namespace std::string_view_literals;

    class engine
    {
        struct node
        {
            tasking::task task;
            std::string   address;
            std::int64_t  id;
        };

        zmq::context_t&         context_;
        std::list<node>         root_nodes_;
        std::set<std::uint16_t> port_pool_;

    public:
        explicit engine(zmq::context_t& context)
            : context_{context}
        {
            for (auto port = std::uint16_t{5500}; port < 5600; ++port)
            {
                port_pool_.insert(port);
            }
        }

        auto static constexpr any_node = std::int64_t{-1};

        /// Pops port from the port pool
        auto pop_port_from_pool() -> std::uint16_t
        {
            port_pool_ability_check();

            //
            // Extract free port
            //
            auto const it   = port_pool_.begin();
            auto const port = *it;

            //
            // Pop it from pool
            //
            port_pool_.erase(it);

            return port;
        }

        /// Creates new node locally
        auto create_node(std::int64_t const id) -> response
        {
            //
            // Create new node parameters
            //
            auto const port    = pop_port_from_pool();
            auto const address = "tcp://*:" + std::to_string(port);
            auto const args    = address + " " + std::to_string(id);

            //
            // Start new task
            //
            auto task = tasking::get_launcher_for({
                .path = std::filesystem::current_path() / "slave.exe"sv,
                .args = args,
            }).start();

            //
            // Register new node
            //
            root_nodes_.push_front({
                .task = std::move(task),
                .address = "tcp://localhost:" + std::to_string(port),
                .id = id,
            });

            //
            // Receive task process ID
            //
            try
            {
                auto response = pid(id);
                if (response.code != response::error::ok)
                {
                    throw_internal("cannot initialize new node");
                }
                return response;
            }
            catch (...)
            {
                //
                //  Something gone wrong, so now we follow next plan:
                //      1. kill created task
                //      2. remove it's entry
                //      3. restore used port
                //      4. throw an code
                //
                root_nodes_.front().task.kill();
                root_nodes_.pop_front();
                port_pool_.insert(port);
                throw;
            }
        }

        /// Sends exec command
        auto exec(std::int64_t const target_id, std::string_view const command) -> response
        {
            auto const request = build_request(target_id, command);
            return ask_every_until_response(target_id, request);
        }

        /// Sends pid command
        auto pid(std::int64_t const target_id) -> response
        {
            auto const request = build_request(target_id, "pid");
            return ask_every_until_response(target_id, request);
        }

        /// Builds request string for the node with given id
        auto static build_request(std::int64_t const id, std::string_view const message) -> std::string
        {
            return std::to_string(id) + " " + std::string{message};
        }

        /// Builds request string for the node with given id
        auto static build_target_request(std::int64_t const target, std::string_view const request) -> std::string
        {
            return std::string{ request } + " " + std::to_string(target);
        }

        /// Creates new node remotely
        [[deprecated]]
        auto create_remote_node(std::int64_t const id, std::int64_t const parent) -> response
        {
            auto const message = "create " + std::to_string(id);
            auto const request = build_request(parent, message);
            return ask_every_until_response(parent, request);
        }

        /// Removes node from current network
        auto remove(std::int64_t const id) -> response
        {
            auto const begin = root_nodes_.begin();
            auto const end   = root_nodes_.end();

            for (auto node = begin; node != end; ++node)
            {
                if (node->id == id)
                {
                    //
                    // Send kill command
                    //
                    ask_every_until_response(id, "kill");

                    //
                    // Erase node entry:
                    //  1. Kill it if it's still not dead
                    //  2. Remove it from node list
                    //
                    node->task.kill();
                    root_nodes_.erase(node);

                    return {.code = response::error::ok};
                }
            }

            auto const request = build_target_request(id, "remove");
            auto const primary = build_request(any_node, request);
            return ask_every_until_response(any_node, primary);
        }

        /// Sends ping command
        [[deprecated]]
        auto ping(std::int64_t const id) -> response
        {
            auto const request = build_request(id, "ping");
            return ask_every_until_response(id, request);
        }

        /// Sends message once to every root node until valuable response
        /**
         * @param message: string that will be sent
         * @return: first valuable response
        */
        auto ask_every_until_response(std::int64_t const id, std::string_view const message) -> response
        {
            /// Sending routine
            auto send = [this, id](node const& node, std::string_view const string) -> response
            {
                //
                // Open socket
                //
                auto socket = zmq::socket_t{context_, ZMQ_REQ};
                socket.setsockopt(ZMQ_RCVTIMEO, 1000);
                socket.connect(node.address);

                //
                // Send prepared message
                //
                auto request = zmq::message_t{string.size()};
                std::memcpy(request.data(), string.data(), string.size());
                socket.send(request, zmq::send_flags::none);

                //
                // Receive response
                //
                auto       response    = zmq::message_t{256};
                auto const recv_result = socket.recv(response, zmq::recv_flags::none);
                if (!recv_result.has_value())
                {
                    return
                    {
                        .code = (id == node.id) ? response::error::unavailable : response::error::invalid_path,
                    };
                }
                response.rebuild(*recv_result);

                response.data();

                //
                // Deserialize response
                //
                auto deserialized_response = network::response{};
                deserialized_response.deserialize_from(response);

                return deserialized_response;
            };

            auto non_valuable_response = response
            {
                .code = response::error::unknown
            };

            //
            // Loop over nearest nodes
            //
            for (auto const& node : root_nodes_)
            {
                auto response = send(node, message);
                if (response.code == response::error::invalid_path)
                {
                    non_valuable_response = network::response
                    {
                        .code = response::error::invalid_path,
                    };

                    continue;
                }
                if (response.code != response::error::unknown)
                {
                    return response;
                }
            }

            //
            // We didn't get any valuable response
            //
            return non_valuable_response;
        }

        /// Throws runtime_error with prefix "Internal code: " 
        [[noreturn]]
        static auto throw_internal(std::string_view const message) noexcept(false) -> void
        {
            throw std::runtime_error{"Internal code: " + std::string{message}};
        }

        /// Checks if there are available ports to pop
        auto port_pool_ability_check() const -> void
        {
            if (port_pool_.empty())
            {
                throw_internal("port pool is empty");
            }
        }
    };
}