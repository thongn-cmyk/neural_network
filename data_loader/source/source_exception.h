#ifndef __SOURCE_EXCEPTION_H__
#define __SOURCE_EXCEPTION_H__

#include <exception>

namespace data_loader::source_exception
{
    struct retryable_error: std::runtime_error
    {
        retryable_error(const char * err_msg): std::runtime_error(err_msg){}
    };

    struct bad_resource_pointer_error: std::invalid_argument
    {
        bad_resource_pointer_error(const char * err_msg): std::invalid_argument(err_msg){}
    };

    struct bad_state_error: std::runtime_error
    {
        bad_state_error(const char * err_msg): std::runtime_error(err_msg){}
    };

    struct soft_file_read_error: retryable_error
    {
        soft_file_read_error(const char * err_msg): retryable_error(err_msg){}
    };

    struct hard_file_read_error: std::runtime_error
    {
        hard_file_read_error(const char * err_msg): std::runtime_error(err_msg){}
    };
}

#endif

