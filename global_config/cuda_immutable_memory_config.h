#ifndef __GLOBAL_CUDA_IMMUTABLE_MEMORY_CONFIG_H__
#define __GLOBAL_CUDA_IMMUTABLE_MEMORY_CONFIG_H__

#include <stdint.h>
#include <stdlib.h>

namespace global_config::cuda_immutable_memory_config
{
    static inline constexpr uint64_t BUMP_ALLOCATION_SZ         = size_t{1} << 20;
    static inline constexpr uint64_t BUMP_ALLOCATION_THRESHOLD  = size_t{1} << 20;
    static inline constexpr uint64_t GLOBAL_CACHE_SZ            = size_t{1} << 30;
}

#endif