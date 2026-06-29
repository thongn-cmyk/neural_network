#ifndef __CUDA_MEMORY_CONFIG_H__
#define __CUDA_MEMORY_CONFIG_H__

#include <stdint.h>
#include <stdlib.h>
#include <optional>

namespace global_config::cuda_memory_config
{
    static inline constexpr uint64_t CUDA_HEAP_MEMORY_SZ    = uint64_t{1} << 28;
    static inline constexpr uint64_t CUDA_HEAP_LEAF_SZ      = size_t{1} << 7;
    static inline constexpr uint64_t CUDA_STACK_MEMORY_SZ   = size_t{1} << 14;
    static inline constexpr bool CUDA_HAS_HEAP              = true;
}

#endif