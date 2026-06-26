#ifndef __CUDA_MANAGEMENT_LOCAL_EXCEPTION_H__
#define __CUDA_MANAGEMENT_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <stdexcept>

namespace cuda_management::local_exception
{
    struct cuda_bad_alloc: std::bad_alloc{};
    
    struct cuda_corruption: std::runtime_error
    {
        inline cuda_corruption(): std::runtime_error("cuda corruption"){}
    };

    struct device_not_available_error: std::invalid_argument
    {
        inline device_not_available_error(const char * msg = "device not available error"): std::invalid_argument(msg){}
    };

    struct cuda_invalid_argument: std::invalid_argument
    {
        inline cuda_invalid_argument(const char * msg): std::invalid_argument(msg){}
    };

    struct cuda_runtime_error: std::runtime_error
    {
        inline cuda_runtime_error(const char * msg): std::runtime_error(msg){}
    };
}

#endif