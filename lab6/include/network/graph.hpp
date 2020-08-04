#pragma once

#include <set>
#include <string_view>

#include <zmq.hpp>

#include <tasking/launcher.hpp>
#include <network/response.hpp>

namespace network {
    using namespace std::string_view_literals;

    /// INLINE class that implements network graph
    class graph {
        struct node {
            tasking::task task;
            std::string   address;
        };

        zmq::context_t&         context_;
        std::list<node>         root_nodes_;
        std::set<std::uint16_t> port_pool_;

    public:
        explicit graph(zmq::context_t& context)
            : context_ {context} {
            for (auto port = std::uint16_t {5500}; port < 5600; ++port) {
                port_pool_.insert(port);
            }
        }

        /// Creates new node
        auto create(std::int64_t const id, std::int64_t const parent) -> response {
            verify(id);
            return x_create_node(id, parent);
        }

        /// Removes node from the network
        auto remove(std::int64_t const id) -> response {
            verify(id);
            return x_remove(id);
        }

        /// Executes command on the node remotely
        auto exec(std::int64_t const id, std::string_view const command) -> response {
            verify(id);
            verify_exec(command);
            return x_exec(id, command);
        }

        /// Pings node
        /**
         * @param id: node unique id
         * @return: ok if succeed otherwise error
        */
        auto ping(std::int64_t const id) -> response {
            verify(id);
            return x_ping(id);
        }

    private:
        auto static constexpr exec_commands = std::array {
            "start"sv,
            "stop"sv,
            "time"sv,
        };

        enum class parent_type {
            root,
            node
        };

        /// Returns parent type via it's id
        [[nodiscard]]
        static auto determine_parent_type(std::int64_t const parent_id) -> parent_type {
            if (parent_id < -1) {
                throw std::invalid_argument {"Invalid parent id"};
            }

            return parent_id == -1 ? parent_type::root : parent_type::node;
        }

        /// Pops port from the port pool
        auto x_pop_port_from_pool() -> std::uint16_t {
            x_port_pool_ability_check();

            auto const it   = port_pool_.begin();
            auto const port = *it;

            port_pool_.erase(it);

            return port;
        }

        /// Creates new node locally
        auto x_create_root_node(std::int64_t const id) -> response {
            //
            // Create new node parameters
            //
            auto const port    = x_pop_port_from_pool();
            auto const address = "tcp://*:" + std::to_string(port);
            auto const args    = address + " " + std::to_string(id);

            //
            // Start new task
            //
            auto task = tasking::get_launcher_for({
                .path = std::filesystem::current_path() / "slave.exe"sv,
                .args = args
            }).start();

            //
            // Register new node
            //
            root_nodes_.push_front({
                .task = std::move(task),
                .address = "tcp://localhost:" + std::to_string(port)
            });

            //
            // Receive task process ID
            //
            try {
                auto response = x_pid(id);
                if (response.error != response::error_type::ok) {
                    x_throw_internal("cannot initialize new node");
                }
                return response;
            }
            catch (...) {
                //
                //  Something gone wrong, so now we follow next plan:
                //      1. kill created task
                //      2. remove it's entry
                //      3. restore used port
                //      4. throw an error
                //
                root_nodes_.front().task.kill();
                root_nodes_.pop_front();
                port_pool_.insert(port);
                throw;
            }
        }

        /// Creates new node remotely
        auto x_create_remote_node(std::int64_t const id, std::int64_t const parent) -> response {
            auto const message = "create " + std::to_string(id);
            auto const request = x_build_request(parent, message);
            return x_ask_every_until_response(request);
        }

        /// Creates new node in the network
        auto x_create_node(std::int64_t const id, std::int64_t const parent) -> response {
            switch (x_ping(id).error) {
            case response::error_type::unknown:
                break;
            default:
                throw std::invalid_argument {"Already exists"};
            }

            switch (determine_parent_type(parent)) {
            case parent_type::root:
                return x_create_root_node(id);
            case parent_type::node:
                return x_create_remote_node(id, parent);
            }

            x_throw_internal("bad parent type");
        }

        /// Sends remove command
        auto x_remove(std::int64_t const id) -> response {
            auto const request = x_build_request(id, "remove");
            return x_ask_every_until_response(request);
        }

        /// Sends exec command
        auto x_exec(std::int64_t const id, std::string_view const command) -> response {
            auto const request = x_build_request(id, command);
            return x_ask_every_until_response(request);
        }

        /// Sends pid command
        auto x_pid(std::int64_t const id) -> response {
            auto const request = x_build_request(id, "pid");
            return x_ask_every_until_response(request);
        }

        /// Sends ping command
        auto x_ping(std::int64_t const id) -> response {
            auto const request = x_build_request(id, "ping");
            return x_ask_every_until_response(request);
        }

        /// Builds request string for the node with given id
        auto static x_build_request(std::int64_t const id, std::string_view const message) -> std::string {
            return std::to_string(id) + " " + std::string {message};
        }

        /// Sends message once to every root node until valuable response
        /**
         * @param message: string that will be sent
         * @return: first valuable response
        */
        auto x_ask_every_until_response(std::string_view const message) -> response {
            /// Projectile of strings over response::error_type
            auto static read_status = [](std::string_view const status) -> response::error_type {
                if (status == "OK") {
                    return response::error_type::ok;
                }
                if (status == "UNKNOWN") {
                    return response::error_type::unknown;
                }
                if (status == "UNAVAILABLE") {
                    return response::error_type::unavailable;
                }
                if (status == "EXISTS") {
                    return response::error_type::exists;
                }

                return response::error_type::bad_request;
            };
            /// Sending routine
            auto send = [this](node const& node, std::string_view const string) -> response {
                //
                // Open socket
                //
                auto socket = zmq::socket_t {context_, ZMQ_REQ};
                socket.connect(node.address);

                //
                // Send prepared message
                //
                auto request = zmq::message_t {string.size()};
                std::memcpy(request.data(), string.data(), string.size());
                socket.send(request, zmq::send_flags::none);

                //
                // Receive answer
                //
                auto       reply       = zmq::message_t {256};
                auto const recv_result = socket.recv(reply, zmq::recv_flags::none);
                reply.rebuild(recv_result.value());
                auto const reply_string = std::string {reply.data<char>(), reply.size()};
                auto       reply_stream = std::istringstream {reply_string};

                //
                // Read STATUS from reply
                //
                auto status_string = std::string {};
                reply_stream >> status_string;
                auto const status = read_status(status_string);

                //
                // Read MESSAGE from reply
                //
                auto message = std::string {};
                reply_stream >> std::ws;
                std::getline(reply_stream, message);

                return {
                    .error = status,
                    .message = std::move(message),
                };
            };

            //
            // Loop over nearest nodes
            //
            for (auto const& node : root_nodes_) {
                auto response = send(node, message);
                if (response.error != response::error_type::unknown) {
                    return response;
                }
            }

            //
            // Return response::error_type::unknown because we didn't get any valuable response
            //
            return {
                .error = response::error_type::unknown
            };
        }

        /// Verifies node id value
        static auto verify(std::int64_t const id) -> void {
            if (id < 0) {
                throw std::invalid_argument {"Invalid id"};
            }
        }

        /// Verifies exec command string
        static auto verify_exec(std::string_view const command) -> void {
            if (!std::any_of(
                exec_commands.begin(),
                exec_commands.end(),
                [&](std::string_view const type) -> bool {
                    return type == command;
                })) {
                throw std::invalid_argument {"Invalid command string"};
            }
        }

        /// Throws runtime_error with prefix "Internal error: " 
        [[noreturn]]
        static auto x_throw_internal(std::string_view const message) noexcept(false) -> void {
            throw std::runtime_error {"Internal error: " + std::string {message}};
        }

        /// Checks if there are available ports to pop
        auto x_port_pool_ability_check() const -> void {
            if (port_pool_.empty()) {
                x_throw_internal("port pool is empty");
            }
        }
    };
}
