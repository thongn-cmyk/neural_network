#ifndef __CUDA_DEVICE_MEMORY_CONFIG_H__
#define __CUDA_DEVICE_MEMORY_CONFIG_H__

#include <stdint.h>

namespace global_config::cuda_device_memory_config
{
    __device__ static constexpr inline uint32_t ON_DEVICE_MEMORY_SZ = uint32_t{1} << 30;
    __device__ static constexpr inline bool USE_SESSION_MEMORY      = true;
}

#endif