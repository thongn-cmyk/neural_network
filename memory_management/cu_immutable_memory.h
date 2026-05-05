#ifndef __MEMORY_MANAGEMENT_CU_IMMUTABLE_MEMORY_H__
#define __MEMORY_MANAGEMENT_CU_IMMUTABLE_MEMORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <string_view>
#include <memory>
#include <cuda_management/host_service_header.h>
#include "immutable_multiplatform_memory_x.h"
#include <stl_extension/stdx.h>

namespace cu_immutable_memory
{
    using MemoryReference           = immutable_multiplatform_memory_x::MemoryReference;
    using CacheAndAcquireArgument   = immutable_multiplatform_memory_x::CacheAndAcquireArgument;

    struct Signature{};

    using SingletonObject = stdx::singleton_container<std::unique_ptr<immutable_multiplatform_memory_x::ImmutableMemoryCacheInterface>, Signature>;

    class InternalAllocator: public virtual immutable_multiplatform_memory_x::MemoryAllocatorInterface
    {
        public:

            auto allocate_from_view(std::string_view buffer_view) -> std::shared_ptr<void>
            {
                return cuda_management::host_service::make_cuda_buffer_from_host_view(buffer_view);
            }
    };

    void init(size_t cache_sz)
    {
        SingletonObject::get() = std::make_unique<immutable_multiplatform_memory_x::ImmutableMemoryCache>(cache_sz);
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void deinit() noexcept
    {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        SingletonObject::get() = nullptr;
    }

    auto get_instance() -> immutable_multiplatform_memory_x::ImmutableMemoryCacheInterface *
    {
        if (SingletonObject::get() == nullptr)
        {
            std::abort();
        }

        return SingletonObject::get().get();
    }

    auto acquire_memory(const std::shared_ptr<void>& immutable_reference) noexcept -> std::optional<MemoryReference>
    {
        std::optional<MemoryReference> rs{};
        get_instance()->acquire_memory(&immutable_reference, 1u, &rs);

        return rs;
    }

    auto cache_and_acquire_memory(const std::shared_ptr<void>& immutable_reference, std::string_view host_memory) -> MemoryReference
    {
        CacheAndAcquireArgument arg
        {
            .immutable_reference    = immutable_reference,
            .host_memory            = host_memory
        };

        MemoryReference  rs{};
        get_instance()->cache_and_acquire_memory(&arg, 1u, &rs);

        return rs;
    }

    void release_memory(MemoryReference memory_reference) noexcept
    {
        get_instance()->release_memory(&memory_reference, 1u);
    }

    void evict_memory(const std::shared_ptr<void>& immutable_reference) noexcept
    {
        get_instance()->evict_memory(&immutable_reference, 1u);
    }

    auto get_cu_memspan(MemoryReference memory_reference) noexcept -> std::pair<void *, size_t>
    {
        return std::make_pair(memory_reference.device_ptr, memory_reference.ptr_mem_sz);
    }
}

#endif