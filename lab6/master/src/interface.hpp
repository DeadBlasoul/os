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

        network::topology::tree::engine& engine_;
        runner_t                         runner_;

    public:
        explicit interface(network::topology::tree::engine& engine)
            : engine_{engine}
        {
            declare_interface();
        }

        auto execute(std::string_view const command) noexcept(false) -> network::response
        {
            auto const response = runner_.execute(command);

            return response.value_or(
                network::response
                {
                    .code = network::response::error::ok
                }
            );
        }

    private:
        auto declare_interface() noexcept(false) -> void;
    };
}
