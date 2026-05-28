#ifndef __CLIENT_BOX_TASK_BOX_LOCAL_EXCEPTION_H__
#define __CLIENT_BOX_TASK_BOX_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <stdexcept>
#include <expected>

namespace client_box::task_box
{
    using local_exception_t = uint8_t;

    struct destroyed_client_box_error: std::invalid_argument
    {
        destroyed_client_box_error(): std::invalid_argument("bad client box operation, client box was destroyed"){}
    };

    struct second_run_error: std::invalid_argument
    {
        second_run_error(): std::invalid_argument("bad client box operation, second run invoked"){}
    };

    struct run_not_invoked_error: std::invalid_argument
    {
        run_not_invoked_error(): std::invalid_argument("bad client box operation, run was not invoked"){}
    };

    static inline constexpr local_exception_t SUCCESS                                   = 0u;
    static inline constexpr local_exception_t DESTROYED_CLIENT_BOX_ERROR_CODE           = 1u;
    static inline constexpr local_exception_t SECOND_RUN_ERROR_CODE                     = 2u;
    static inline constexpr local_exception_t RUN_NOT_INVOKED_ERROR_CODE                = 3u;
}

#endif