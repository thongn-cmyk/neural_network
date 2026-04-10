#ifndef __DG_DEVIATION_PROJECTION_SERVER_LOCAL_EXCEPTION_H__
#define __DG_DEVIATION_PROJECTION_SERVER_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <expected>
#include <exception>
#include <stdexcept>

namespace deviation_projection_server
{
    using local_exception_t = uint8_t;

    static inline constexpr local_exception_t SUCCESS                       = 0u;
    static inline constexpr local_exception_t INVALID_ARGUMENT_ERROR_CODE   = 1u;
    static inline constexpr local_exception_t RUNTIME_ERROR_CODE            = 2u;
    static inline constexpr local_exception_t CLIENT_NOT_FOUND_ERROR_CODE   = 3u;

    struct local_invalid_argument: std::invalid_argument
    {
        local_invalid_argument(const char * msg = "invalid argument"): std::invalid_argument(msg){}
    };

    struct local_runtime_error: std::runtime_error
    {
        local_runtime_error(const char * msg = "runtime error"): std::runtime_error(msg){}
    };

    struct client_not_found_error: std::invalid_argument
    {
        client_not_found_error(): std::invalid_argument("bad client box, client_box id not found"){}
    };
}

#endif