#pragma once

#include <string>

namespace network {
    struct response {
        enum class error_type {
            ok,
            unknown,
            unavailable,
            bad_request,
            exists
        };

        error_type  error;
        std::string message;
    };
}
