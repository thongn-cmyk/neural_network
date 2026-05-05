#ifndef __IMMUTABLE_MULTIPLATFORM_MEMORY_X_H__
#define __IMMUTABLE_MULTIPLATFORM_MEMORY_X_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include "key_heap.h"
#include <memory>
#include <string>
#include <string_view>
#include <stl_extension/unordered_node_map.h>
#include <mutex_extension/fair_mutex.h>

namespace immutable_multiplatform_memory_x
{
    class MemoryAllocatorInterface
    {
        public:
            
            virtual ~MemoryAllocatorInterface() noexcept = default;

            virtual auto allocate_from_view(std::string_view buffer_view) -> std::shared_ptr<void> = 0;
    }; 

    struct MemoryReference
    {
        void * device_ptr;
        size_t ptr_mem_sz;
        uintptr_t internal_key;
    };

    struct CacheAndAcquireArgument
    {
        std::shared_ptr<void> immutable_reference;
        std::string_view host_memory;
    };

    class ImmutableMemoryCacheInterface
    {
        public:

            virtual ~ImmutableMemoryCacheInterface() noexcept = default;

            virtual void acquire_memory(const std::shared_ptr<void> * immutable_reference_arr,
                                        size_t immutable_reference_arr_sz,
                                        std::optional<MemoryReference> * output_arr) = 0;

            virtual void cache_and_acquire_memory(const CacheAndAcquireArgument * arg_arr, size_t arg_arr_sz,
                                                  MemoryReference * result_arr) = 0;

            virtual void release_memory(MemoryReference * reference_arr, size_t reference_arr_sz) noexcept = 0;

            virtual void evict_memory(const std::shared_ptr<void> * immutable_reference_arr,
                                      size_t immutable_reference_arr_sz) noexcept = 0;

            virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct MemoryNode
    {
        std::shared_ptr<void> immutable_reference;
        std::shared_ptr<void> device_mem_ptr;
        size_t device_mem_ptr_sz;
        size_t last_updated;
        size_t reference_counter;
    };

    struct MemoryNodeComparator
    {
        constexpr auto operator()(const MemoryNode& lhs, const MemoryNode& rhs) const noexcept -> bool
        {
            if (lhs.reference_counter < rhs.reference_counter)
            {
                return true;
            }

            if (lhs.reference_counter > rhs.reference_counter)
            {
                return false;
            }

            if (lhs.last_updated < rhs.last_updated)
            {
                return true;
            }

            if (lhs.last_updated > rhs.last_updated)
            {
                return false;
            }

            return true;
        }
    };

    struct MemoryNodeKeyExtractor
    {
        constexpr auto operator()(const MemoryNode& val) const noexcept -> uintptr_t
        {
            return reinterpret_cast<uintptr_t>(val.immutable_reference.get());
        }
    };

    class FreeAllocationHolder
    {
        private:

            std::vector<std::shared_ptr<void>> free_vec;
        
        public:

            void push(const std::shared_ptr<void>& mem)
            {
                this->free_vec.push_back(mem);
            }
    };

    class ImmutableMemoryCache: public virtual ImmutableMemoryCacheInterface
    {
        private:

            using KeyHeap               = immutable_multiplatform_memory_x::key_heap::KeyHeap<MemoryNode, MemoryNodeComparator, MemoryNodeKeyExtractor>;
            using local_unordered_set   = unordered_map_variants::unordered_node_set<uintptr_t, size_t, hasher::default_hasher<uintptr_t>>;

            KeyHeap management_heap;

            local_unordered_set wanted_eviction_set;
            std::shared_ptr<MemoryAllocatorInterface> allocator;

            size_t occupied_memory_sz;
            size_t auto_evict_memory_threshold;
            size_t self_paced_clock;

            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

            static inline constexpr size_t MAX_CONSUME_SZ = size_t{1} << 8;

        public:

            ImmutableMemoryCache(std::shared_ptr<MemoryAllocatorInterface> allocator,
                                 size_t auto_evict_memory_threshold): management_heap(),
                                                                      wanted_eviction_set(),
                                                                      allocator(std::move(allocator)),
                                                                      occupied_memory_sz(0u),
                                                                      auto_evict_memory_threshold(auto_evict_memory_threshold),
                                                                      self_paced_clock(0u),
                                                                      mtx(fair_mutex::make_unique_fair_atomic_flag())
            {
                if (this->allocator == nullptr)
                {
                    throw std::invalid_argument("bad allocator, null");
                }
            }

            void acquire_memory(const std::shared_ptr<void> * immutable_reference_arr, size_t sz,
                                std::optional<MemoryReference> * output_arr)
            {
                if (sz > MAX_CONSUME_SZ)
                {
                    std::abort();
                }

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i)
                {
                    output_arr[i] = this->acquire_memory_one(immutable_reference_arr[i]);
                }
            }

            void cache_and_acquire_memory(const CacheAndAcquireArgument * arg_arr, size_t sz,
                                          MemoryReference * result_arr)
            {
                if (sz > MAX_CONSUME_SZ)
                {
                    std::abort();
                }

                FreeAllocationHolder free_holder{};

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    size_t success_sz = 0u;

                    try
                    {
                        for (size_t i = 0u; i < sz; ++i)
                        {
                            result_arr[success_sz++] = this->cache_and_acquire_memory_one(arg_arr[i], free_holder);
                        }
                    }
                    catch (...)
                    {
                        this->release_memory(result_arr, success_sz);
                        throw;
                    }
                }
            }

            void release_memory(MemoryReference * reference_arr, size_t sz) noexcept
            {
                if (sz > MAX_CONSUME_SZ)
                {
                    std::abort();
                }

                FreeAllocationHolder free_holder{};

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        this->release_memory_one(reference_arr[i], free_holder);
                    }

                    this->auto_evict(free_holder);
                }
            }

            void evict_memory(const std::shared_ptr<void> * immutable_reference_arr, size_t sz) noexcept
            {
                if (sz > MAX_CONSUME_SZ)
                {
                    std::abort();
                }

                FreeAllocationHolder free_holder{};

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        this->evict_memory_one(immutable_reference_arr[i], free_holder);
                    }
                }
            }

            auto max_consume_size() noexcept -> size_t
            {
                return MAX_CONSUME_SZ;
            }

        private:

            auto acquire_memory_one(const std::shared_ptr<void>& immutable_reference) noexcept -> std::optional<MemoryReference>
            {
                if (immutable_reference == nullptr)
                {
                    std::abort();
                }

                uintptr_t key                       = reinterpret_cast<uintptr_t>(immutable_reference.get());
                const MemoryNode * mem_node         = this->management_heap.get_by_key(key);

                if (mem_node == nullptr)
                {
                    return std::nullopt;
                }

                MemoryReference rs                  =
                {
                    .device_ptr     = mem_node->device_mem_ptr.get(),
                    .ptr_mem_sz     = mem_node->device_mem_ptr_sz,
                    .internal_key   = key 
                };

                auto mutator                        = [&](MemoryNode& mem_node) noexcept
                {
                    mem_node.last_updated       = this->self_paced_clock++;
                    mem_node.reference_counter  += 1;
                };

                this->management_heap.update_by_key(key, mutator);

                return rs;
            }

            auto cache_and_acquire_memory_one(const CacheAndAcquireArgument& arg,
                                              FreeAllocationHolder& free_holder) -> MemoryReference
            {
                std::optional<MemoryReference> rs = this->acquire_memory_one(arg.immutable_reference);

                if (rs.has_value())
                {
                    return rs.value();
                }

                if (arg.immutable_reference == nullptr)
                {
                    std::abort();
                }

                if (this->occupied_memory_sz >= this->auto_evict_memory_threshold)
                {
                    this->auto_evict(free_holder);
                }

                std::shared_ptr<void> device_mem_ptr    = this->allocator->allocate_from_view(arg.host_memory);
                size_t device_mem_ptr_mem_sz            = arg.host_memory.size();
                size_t last_updated                     = this->self_paced_clock++;
                size_t reference_counter                = 1u;

                MemoryNode mem_node                     = MemoryNode
                {
                    .immutable_reference    = arg.immutable_reference,
                    .device_mem_ptr         = device_mem_ptr,
                    .device_mem_ptr_sz      = device_mem_ptr_mem_sz,
                    .last_updated           = last_updated,
                    .reference_counter      = reference_counter
                };

                this->management_heap.push(mem_node);
                this->occupied_memory_sz                 += arg.host_memory.size();

                return MemoryReference
                {
                    .device_ptr     = device_mem_ptr.get(),
                    .ptr_mem_sz     = device_mem_ptr_mem_sz,
                    .internal_key   = reinterpret_cast<uintptr_t>(arg.immutable_reference.get())
                };
            }

            void release_memory_one(MemoryReference memory_reference,
                                    FreeAllocationHolder& free_holder) noexcept
            {
                size_t last_reference_counter;

                auto mutator    = [&](MemoryNode& mem_node) noexcept
                {
                    if (mem_node.reference_counter == 0u)
                    {
                        std::abort();
                    }

                    mem_node.reference_counter  -= 1;
                    last_reference_counter      = mem_node.reference_counter;
                };

                this->management_heap.update_by_key(memory_reference.internal_key, mutator);

                if (last_reference_counter == 0u)
                {
                    auto set_ptr = this->wanted_eviction_set.find(memory_reference.internal_key);

                    if (set_ptr != this->wanted_eviction_set.end())
                    {   
                        free_holder.push(this->management_heap.get_by_key(memory_reference.internal_key)->device_mem_ptr);

                        this->occupied_memory_sz -= memory_reference.ptr_mem_sz;
                        this->management_heap.erase_by_key(memory_reference.internal_key);
                        this->wanted_eviction_set.erase(set_ptr);
                    }
                }
            }

            void auto_evict(FreeAllocationHolder& free_holder) noexcept
            {
                while (true)
                {
                    if (this->occupied_memory_sz < this->auto_evict_memory_threshold)
                    {
                        return;
                    }

                    if (this->management_heap.empty())
                    {
                        return;
                    }

                    const MemoryNode& mem_node = this->management_heap.peek();

                    if (mem_node.reference_counter != 0u)
                    {
                        return;
                    }

                    free_holder.push(mem_node.device_mem_ptr);

                    this->occupied_memory_sz -= mem_node.device_mem_ptr_sz;
                    this->management_heap.erase_by_key(reinterpret_cast<uintptr_t>(mem_node.immutable_reference.get()));
                }
            }

            void evict_memory_one(const std::shared_ptr<void>& immutable_reference,
                                  FreeAllocationHolder& free_holder)
            {
                uintptr_t key                       = reinterpret_cast<uintptr_t>(immutable_reference.get());
                const MemoryNode * mem_node         = this->management_heap.get_by_key(key);

                if (mem_node == nullptr)
                {
                    return;
                }

                if (mem_node->reference_counter == 0u)
                {
                    free_holder.push(mem_node->device_mem_ptr);

                    this->occupied_memory_sz -= mem_node->device_mem_ptr_sz;
                    this->management_heap.erase_by_key(key);

                    return;
                }

                this->wanted_eviction_set.insert(key); //bind lifetime of this -> that of memory reference node, upon 0 exit
            }
    };
}

#endif