#ifndef __CUDA_MEMORY_CONFIG_H__
#define __CUDA_MEMORY_CONFIG_H__

#include <stdint.h>
#include <stdlib.h>
#include <optional>

namespace global_config::cuda_memory_config
{
    static inline constexpr uint64_t CUDA_HEAP_MEMORY_SZ    = uint64_t{1} << 32;
    static inline constexpr uint64_t CUDA_HEAP_LEAF_SZ      = size_t{1} << 8;
    static inline constexpr bool CUDA_HAS_HEAP              = true;
}

#endif