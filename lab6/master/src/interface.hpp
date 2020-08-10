#pragma once

#include <utility/commandline.hpp>
#include <network/response.hpp>
#include <network/topology.hpp>

namespace executable
{
    class interface
    {
        using response_t = network::response;
        using argv_t = utility::commandline::argv_t;
        using runner_t = utility::commandline::runner<response_t(argv_t)>;

        std::set<std::uint16_t>          port_pool_;
        network::topology::tree::engine& engine_;
        runner_t                         runner_;

    public:
        explicit interface(network::topology::tree::engine& engine)
            : engine_{ engine }
        {
            init_pool();
            declare();
        }

        auto execute(std::string_view const command) noexcept(false) -> network::response
        {
            auto const response = runner_.execute(command);

            return response.value_or(
                network::response
                {
                    .error = network::error::ok
                }
            );
        }

    private:
        auto declare() noexcept(false) -> void;

        auto init_pool() noexcept -> void
        {
            for (auto port = std::uint16_t{ 5500 }; port < 5600; ++port)
            {
                port_pool_.insert(port);
            }
        }

        /// Pops port from the port pool
        auto pop_port_from_pool() -> std::uint16_t
        {
            port_pool_ability_check();

            //
            // Extract free port
            //
            auto const it = port_pool_.begin();
            auto const port = *it;

            //
            // Pop it from pool
            //
            port_pool_.erase(it);

            return port;
        }

        /// Checks if there are available ports to pop
        auto port_pool_ability_check() const -> void
        {
            if (port_pool_.empty())
            {
                throw std::runtime_error{ "port pool is empty" };
            }
        }
    };
}
