#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include "cuda_malloc.h"
#include "generic_allocator.h"
#include <global_config/cuda_memory_config.h>
#include <stl_extension/stdx.h>
#include <memory>
#include <type_traits>
#include <iostream>
#include <cuda_management/local_exception.h>

namespace cuda_management::cuda_malloc
{
    struct Signature{};

    using SingletonContainer = stdx::singleton_container<std::unique_ptr<AllocatorInterface>, Signature>;

    extern void init()
    {
        stdx::memtransaction_guard tx_grd;

        cudaError_t status  = cudaDeviceSetLimit(cudaLimitStackSize,
                                                 global_config::cuda_memory_config::CUDA_STACK_MEMORY_SZ);

        if (status != cudaSuccess)
        {
            throw cuda_management::local_exception::cuda_invalid_argument(cudaGetErrorString(status));
        }

        if (global_config::cuda_memory_config::CUDA_HAS_HEAP)
        {
            SingletonContainer::get() = std::make_unique<GenericAllocator>(GenericAllocatorConfig
            {
                .config = DedicatedAllocatorConfig
                {
                    .heap_memory_sz = global_config::cuda_memory_config::CUDA_HEAP_MEMORY_SZ,
                    .heap_leaf_sz   = global_config::cuda_memory_config::CUDA_HEAP_LEAF_SZ
                }
            });
        }
        else
        {
            SingletonContainer::get() = std::make_unique<GenericAllocator>(GenericAllocatorConfig
            {
                .config = NormalAllocatorConfig{}
            });
        }
    }

    extern void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        SingletonContainer::get() = nullptr;
    }

    static auto get_instance() -> AllocatorInterface *
    {
        if (SingletonContainer::get() == nullptr)
        {
            std::abort();
        }

        return SingletonContainer::get().get();
    }

    extern auto malloc(size_t sz) -> void *
    {
        return get_instance()->malloc(sz);
    }

    extern void free(void * ptr) noexcept
    {
        get_instance()->free(ptr);
    }
}