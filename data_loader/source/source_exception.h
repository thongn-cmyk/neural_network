#ifndef __DATA_LOADER_SOURCE_EXCEPTION_H__
#define __DATA_LOADER_SOURCE_EXCEPTION_H__

#include <exception>
#include <data_loader/exception_base.h>

namespace data_loader::source::source_exception
{
    using namespace data_loader::exception_base;

    struct bad_resource_pointer_error: invalid_argument_base
    {
        bad_resource_pointer_error(const char * err_msg = "bad resource pointer error",
                                   const char * id = "bad_resource_pointer_error"): invalid_argument_base(err_msg, id){}
    };

    struct authentication_error: invalid_argument_base
    {
        authentication_error(const char * err_msg = "authentication error",
                             const char * id = "authentication_error"): invalid_argument_base(err_msg, id){}
    };

    struct bad_state_error: runtime_error_base
    {
        bad_state_error(const char * err_msg = "bad state error",
                        const char * id = "bad_state_error"): runtime_error_base(err_msg, id){}
    };

    struct soft_file_read_error: retryable_error
    {
        soft_file_read_error(const char * err_msg = "soft file read error",
                             const char * id = "soft_file_read_error"): retryable_error(err_msg, id){}
    };

    struct connection_error: retryable_error
    {
        connection_error(const char * err_msg = "connection error",
                         const char * id = "connection_error"): retryable_error(err_msg, id){}
    };

    struct hard_file_read_error: runtime_error_base
    {
        hard_file_read_error(const char * err_msg = "hard file read error",
                             const char * id = "hard_file_read_error"): runtime_error_base(err_msg, id){}
    };

    struct other_error: runtime_error_base
    {
        other_error(const char * err_msg = "other error",
                    const char * id = "other_error"): runtime_error_base(err_msg, id){}
    };

    struct source_invalid_argument: invalid_argument_base
    {
        source_invalid_argument(const char * err_msg = "source invalid argument",
                                const char * id = "source_invalid_argument"): invalid_argument_base(err_msg, id){}
    };
}

#endif

