#pragma once

#include <set>
#include <string_view>

#include <zmq.hpp>

#include <tasking/launcher.hpp>
#include <network/request.hpp>
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

        zmq::context_t& context_;
        std::list<node> root_nodes_;

    public:
        explicit engine(zmq::context_t& context)
            : context_{context}
        {
        }

        auto static constexpr any_node = std::int64_t{-1};


        /// Creates new node locally
        auto create_node(std::int64_t const id, std::int16_t const port) -> response
        {
            //
            // Create new node parameters
            //
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
                if (response.error != error::ok)
                {
                    if (response.message.empty())
                    {
                        throw std::runtime_error{"cannot create new node"};
                    }
                    throw std::runtime_error{response.message};
                }
                return response;
            }
            catch (...)
            {
                //
                //  Something gone wrong, so now we follow next plan:
                //      1. kill created task
                //      2. remove it's entry
                //      3. throw an error
                //
                root_nodes_.front().task.kill();
                root_nodes_.pop_front();
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
            return std::string{request} + " " + std::to_string(target);
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
                    auto const kill_request = build_request(id, "kill");
                    ask_every_until_response(id, kill_request);

                    //
                    // Erase node entry:
                    //  1. Kill it if it's still not dead
                    //  2. Remove it from node list
                    //
                    node->task.kill();
                    root_nodes_.erase(node);

                    return {.error = error::ok};
                }
            }

            auto const request = build_target_request(id, "remove");
            auto const primary = build_request(any_node, request);
            return ask_every_until_response(any_node, primary);
        }

        /// Sends message once to every root node until valuable response
        /**
         * @param message: string that will be sent
         * @param target_id: target node id
         * @return: first valuable response
        */
        auto ask_every_until_response(std::int64_t const target_id, std::string_view const message) -> response
        {
            auto recv_response = [target_id](zmq::socket_t& socket, std::int64_t const node_id) -> response
            {
                auto       response = zmq::message_t{256};
                auto const result   = socket.recv(response, zmq::recv_flags::none);
                if (!result.has_value())
                {
                    return
                    {
                        .error = target_id == node_id ? error::unavailable : error::invalid_path ,
                    };
                }
                response.rebuild(*result);

                auto deserialized_response = network::response{};
                deserialized_response.deserialize_from(response);

                return deserialized_response;
            };

            /// Low-level sending routine
            auto static send_request = [&](zmq::socket_t& socket, request const& request, std::int64_t const node_id) -> response
            {
                auto serialized_request = zmq::message_t{request.size()};
                request.serialize_to(serialized_request);
                socket.send(serialized_request, zmq::send_flags::none);

                return recv_response(socket, node_id);
            };

            /// Sending routine
            auto send = [this](node const& node, std::string_view const string) -> response
            {
                //
                // Open envelope socket
                //
                auto socket = zmq::socket_t{context_, ZMQ_REQ};
                socket.setsockopt(ZMQ_RCVTIMEO, 1000);
                socket.connect(node.address);

                //
                // Send envelope
                //
                if (auto response = send_request(socket, {.type = request::type::envelope}, node.id);
                    response.error != error::ok)
                {
                    return response;
                }

                //
                // Open message socket
                //
                socket = zmq::socket_t{context_, ZMQ_REQ};
                socket.setsockopt(ZMQ_RCVTIMEO, 30000);
                socket.connect(node.address);

                //
                // Send message
                //
                auto response = send_request(
                    socket, 
                    {
                        .type = request::type::message,
                        .message = std::string{string}
                    }, 
                    node.id);

                //
                // Return response from last connection
                //
                return response;
            };

            auto non_valuable_response = response
            {
                .error = error::unknown
            };

            //
            // Loop over nearest nodes
            //
            for (auto const& node : root_nodes_)
            {
                auto response = send(node, message);
                if (response.error == error::invalid_path)
                {
                    non_valuable_response = network::response
                    {
                        .error = error::invalid_path,
                    };

                    continue;
                }
                if (response.error != error::unknown)
                {
                    return response;
                }
            }

            //
            // We didn't get any valuable response
            //
            return non_valuable_response;
        }

        [[nodiscard]]
        auto size() const noexcept -> std::size_t
        {
            return root_nodes_.size();
        }

    private:
        /// Throws runtime_error with prefix "Internal error: " 
        [[noreturn]]
        static auto throw_internal(std::string_view const message) noexcept(false) -> void
        {
            throw std::runtime_error{"Internal error: " + std::string{message}};
        }
    };
}
