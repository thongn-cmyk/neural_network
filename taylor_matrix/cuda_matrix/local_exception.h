#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_LOCAL_EXCEPTION_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>

namespace taylor_matrix::cuda_matrix::local_exception
{
    using local_exception_t = uint32_t;

    static inline constexpr local_exception_t SUCCESS                                   = 0u;
    static inline constexpr local_exception_t OUT_OF_BOUND_ACCESS_CODE                  = 1u;
    static inline constexpr local_exception_t INSUFFICIENT_LOGIT_VEC_SIZE_CODE          = 2u;
    static inline constexpr local_exception_t WAITING_KERNEL_COMPLETE_CODE              = 3u;
    static inline constexpr local_exception_t BAD_CUDA_SYNCHRONIZATION_CODE             = 4u;
    static inline constexpr local_exception_t CUDA_DEVICE_NOT_SUPPORTED_CODE            = 5u;
    static inline constexpr local_exception_t OTHER_INVALID_ARGUMENT_CODE               = 6u;
    static inline constexpr local_exception_t OTHER_RUNTIME_ERROR_CODE                  = 7u;
}

#endif