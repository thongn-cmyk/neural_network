#ifndef __CUDA_MANAGEMENT_CUDA_MALLOC_CUDA_MALLOC_H__
#define __CUDA_MANAGEMENT_CUDA_MALLOC_CUDA_MALLOC_H__

#include <stdint.h>
#include <stdlib.h>

namespace cuda_management::cuda_malloc
{
    extern void init();
    extern void deinit() noexcept;

    extern auto malloc(size_t mem_sz) -> void *;
    extern void free(void * mem_blk) noexcept;
}

#endif