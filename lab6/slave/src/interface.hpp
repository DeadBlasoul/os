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

        volatile std::int64_t const&     id_;
        network::topology::tree::engine& engine_;
        runner_t                         runner_;
        bool                             killed_{false};

    public:
        explicit interface(network::topology::tree::engine& engine, std::int64_t const& id)
            : id_{id}
            , engine_{engine}
        {
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

        auto kill_requested() const noexcept -> bool
        {
            return killed_;
        }

    private:
        auto declare() noexcept(false) -> void;
    };
}
