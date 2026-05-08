#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_LOCAL_HOST_EXCEPTION_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_LOCAL_HOST_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <stdexcept>

namespace taylor_matrix::cuda_matrix::local_exception
{
    struct out_of_bound_access: std::invalid_argument
    {
        out_of_bound_access(): std::invalid_argument("out of bound access"){}
    };

    struct insufficient_logit_vec_size: std::invalid_argument
    {
        insufficient_logit_vec_size(): std::invalid_argument("insufficient logit vec size"){}
    };

    struct waiting_kernel_complete: std::runtime_error
    {
        waiting_kernel_complete(): std::runtime_error("waiting kernel complete"){}        
    };

    struct bad_cuda_synchronization: std::runtime_error
    {
        bad_cuda_synchronization(const char * msg = "bad cuda synchronization"): std::runtime_error(msg){}
    };

    struct cuda_device_not_supported: std::invalid_argument
    {
        cuda_device_not_supported(): std::invalid_argument("cuda device not supported"){}
    };

    struct other_invalid_argument: std::invalid_argument
    {
        other_invalid_argument(const char * msg): std::invalid_argument(msg){}
    };

    struct other_runtime_error: std::runtime_error
    {
        other_runtime_error(const char * msg): std::runtime_error(msg){}
    };

    void throw_error_code(local_exception_t err)
    {
        switch (err)
        {
            case SUCCESS:
            {
                break;
            }
            case OUT_OF_BOUND_ACCESS_CODE:
            {
                throw out_of_bound_access();
            }
            case INSUFFICIENT_LOGIT_VEC_SIZE_CODE:
            {
                throw insufficient_logit_vec_size();
            }
            case WAITING_KERNEL_COMPLETE_CODE:
            {
                throw waiting_kernel_complete();
            }
            case BAD_CUDA_SYNCHRONIZATION_CODE:
            {
                throw bad_cuda_synchronization();
            }
            case CUDA_DEVICE_NOT_SUPPORTED_CODE:
            {
                throw cuda_device_not_supported();
            }
            case OTHER_INVALID_ARGUMENT_CODE:
            {
                throw other_invalid_argument("other invalid argument");
            }
            case OTHER_RUNTIME_ERROR_CODE:
            {
                throw other_runtime_error("other runtime error");
            }
            default:
            {
                throw other_runtime_error("undefined error code");
            }
        }
    }
}

#endif