#ifndef __GLOBAL_TAYLOR_CUDA_MATRIX_CONFIG_H__
#define __GLOBAL_TAYLOR_CUDA_MATRIX_CONFIG_H__

#include <stdint.h>
#include <stdlib.h>

namespace global_config::taylor_cuda_matrix_config
{
    static inline constexpr size_t BUMP_ALLOCATION_SZ           = size_t{1} << 20;
    static inline constexpr size_t BUMP_ALLOCATION_THRESHOLD    = size_t{1} << 20;
}

#endif