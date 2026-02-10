#ifndef __CU_IMMUTABLE_MEMORY_H__
#define __CU_IMMUTABLE_MEMORY_H__

#include "cu_x.h"
#include "immutable_memory.h"

namespace cu_immutable_memory
{
    using MemoryReference = immutable_memory::MemoryReference;

    struct Signature{};

    using SingletonObject = stdx::singleton_container<std::unique_ptr<immutable_memory::ExternalImmutableMemoryCacheInterface>, Signature>;

    static inline const std::chrono::nanoseconds MEMORY_LIFETIME    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1));
    static inline const std::chrono::nanoseconds CRON_PERIODIC_DUR  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));

    class InternalAllocator: public virtual immutable_memory::MemoryAllocatorInterface
    {
        public:

            auto allocate_from_view(std::string_view buffer_view) -> std::shared_ptr<void>
            {
                return cu_x::make_cuda_buffer_from_host_view(buffer_view);
            }
    };

    void init(size_t cache_sz_per_instance,
              size_t concurrent_sz)
    {
        SingletonObject::get() = immutable_memory::Factory::get_normal_immutable_memory_cache(MEMORY_LIFETIME,
                                                                                              CRON_PERIODIC_DUR,
                                                                                              cache_sz_per_instance,
                                                                                              concurrent_sz,
                                                                                              std::make_unique<InternalAllocator>());

        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void deinit() noexcept
    {
        std::atomic_thread_fence(std::memory_order_seq_cst);

        SingletonObject::get() = nullptr;
    }

    auto acquire_memory(const std::shared_ptr<void>& immutable_reference) noexcept -> std::optional<MemoryReference>
    {
        return SingletonObject::get()->acquire_memory(immutable_reference);
    }

    auto cache_n_acquire_memory(const std::shared_ptr<void>& immutable_reference, std::string_view mem_view) -> MemoryReference
    {
        return SingletonObject::get()->cache_n_acquire_memory(immutable_reference, mem_view);
    }

    void release_memory(const MemoryReference& memory_reference) noexcept
    {
        SingletonObject::get()->release_memory(memory_reference);
    }

    auto get_cu_memspan(const MemoryReference& memory_reference) noexcept -> std::pair<void *, size_t>
    {
        return std::make_pair(memory_reference.device_ptr, memory_reference.ptr_mem_sz);
    }
}

#endif