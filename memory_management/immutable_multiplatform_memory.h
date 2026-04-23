#ifndef __IMMUTABLE_MULTIPLATFORM_MEMORY_H__
#define __IMMUTABLE_MULTIPLATFORM_MEMORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <cron_subsystem/cron_subsystem.h>
#include <logging_subsystem/logging_subsystem.h>
#include <chrono>
#include <random>
#include <memory>
#include <algorithm_extension/short_heap.h>

namespace immutable_multiplatform_memory
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

    //today we'd work on 1: chunk memory allocation for MemoryAllocator, somewhat an over-allocated chunk to increase sequential locality + decrease memory fragmentation
    //says that we'd allocate 10x the buffer view and store that for the next whatever allocation, and use shared pointer on the original buffer to increase reference counting and deallocation
    //batching + shorten the immutable memory tree + run this on HIP

    //this is because this optimization strategy is independent of fattening the immutable memory buffer, so I guess we'd have to apply this optimization strategy because
    //most of the time we just converting a vector of immutable memories -> device memories

    //MatrixAsDeviation is a hack that actually only extracts the logit vector and apply their own "magical" logics to do all the deviation in device code
    //this is the only point that breaks symmetric in our designs but we'd have to live with that

    //I guess the reason that we settled for a version of high-thruput pinned memory is because we'd want to be able to use the benefits of pinned memory and their friends, that's all
    //the only thing I dont like is the extend_lifetime and get_expired_memory_vector

    //I think that the auto_evict_memory_sz should be checked upon entry and exit

    //let's do that

    class ExternalImmutableMemoryCacheInterface
    {
        public:

            virtual ~ExternalImmutableMemoryCacheInterface() noexcept = default;

            virtual auto acquire_memory(const std::shared_ptr<void>& immutable_reference) noexcept -> std::optional<MemoryReference> = 0;
            virtual auto cache_n_acquire_memory(const std::shared_ptr<void>& immutable_reference, std::string_view mem_view) -> MemoryReference = 0;
            virtual void release_memory(const MemoryReference& memory_reference) noexcept = 0;
    };

    class ImmutableMemoryCacheInterface: public virtual ExternalImmutableMemoryCacheInterface
    {
        public:

            virtual ~ImmutableMemoryCacheInterface() noexcept = default;

            virtual auto evict_memory(const std::shared_ptr<void>& immutable_reference) noexcept -> bool = 0;
    };

    class MemoryLifetimeManagerInterface
    {
        public:

            virtual ~MemoryLifetimeManagerInterface() noexcept = default;

            virtual void extend_lifetime(const std::shared_ptr<void>& immutable_reference, std::chrono::nanoseconds lifetime) = 0;
            virtual auto get_expired_memory_vector() -> std::vector<std::shared_ptr<void>> = 0;
    };

    class CudaImmutableMemoryCache: public virtual ImmutableMemoryCacheInterface
    {
        private:

            struct HeapNode
            {
                std::shared_ptr<void> immutable_reference;
                std::shared_ptr<void> cuda_mem_ptr;
                size_t cuda_mem_ptr_sz;
                std::chrono::time_point<std::chrono::system_clock> last_updated;
                size_t reference_counter;
                size_t heap_idx;
                uintptr_t reverse_reference;
            };

            std::shared_ptr<MemoryAllocatorInterface> allocator;
            std::unordered_map<uintptr_t, HeapNode *> reference_map;
            std::vector<std::unique_ptr<HeapNode>> heap_node_vec;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            size_t auto_evict_memory_sz;
            size_t auto_evict_memory_threshold;

        public:

            CudaImmutableMemoryCache(std::shared_ptr<MemoryAllocatorInterface> allocator,
                                     size_t auto_evict_memory_threshold): reference_map(),
                                                                          heap_node_vec(),
                                                                          mtx(fair_mutex::make_unique_fair_atomic_flag()),
                                                                          auto_evict_memory_sz(0u),
                                                                          auto_evict_memory_threshold(auto_evict_memory_threshold)
            {
                if (allocator == nullptr)
                {
                    throw std::invalid_argument("bad allocator, null");
                }

                this->allocator = std::move(allocator);
            }

            auto acquire_memory(const std::shared_ptr<void>& immutable_reference) noexcept -> std::optional<MemoryReference>
            {
                if (immutable_reference.get() == nullptr)
                {
                    std::abort();
                }

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                uintptr_t reference_addr    = reinterpret_cast<uintptr_t>(immutable_reference.get());
                auto map_ptr                = this->reference_map.find(reference_addr);

                if (map_ptr == this->reference_map.end())
                {
                    return std::nullopt;
                }

                map_ptr->second->reference_counter   += 1;
                map_ptr->second->last_updated        = std::chrono::system_clock::now();

                this->update_heap_node(map_ptr->second);

                return MemoryReference
                {
                    .device_ptr     = map_ptr->second->cuda_mem_ptr.get(),
                    .ptr_mem_sz     = map_ptr->second->cuda_mem_ptr_sz,
                    .internal_key   = reference_addr
                };
            }

            auto cache_n_acquire_memory(const std::shared_ptr<void>& immutable_reference, std::string_view mem_view) -> MemoryReference
            {
                if (immutable_reference.get() == nullptr)
                {
                    std::abort();
                }

                std::shared_ptr<void> cu_mem = this->allocator->allocate_from_view(mem_view);

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                uintptr_t reference_addr    = reinterpret_cast<uintptr_t>(immutable_reference.get());
                auto map_ptr                = this->reference_map.find(reference_addr);

                if (map_ptr == this->reference_map.end())
                {
                    this->auto_evict();
                    this->heap_node_vec.push_back(std::make_unique<HeapNode>
                        (
                            HeapNode
                            {
                                .immutable_reference    = immutable_reference,
                                .cuda_mem_ptr           = cu_mem,
                                .cuda_mem_ptr_sz        = static_cast<size_t>(mem_view.size()),
                                .last_updated           = std::chrono::system_clock::now(),
                                .reference_counter      = size_t{0u},
                                .heap_idx               = static_cast<size_t>(this->heap_node_vec.size()),
                                .reverse_reference      = reference_addr
                            }
                        )
                    );

                    HeapNode * heap_addr = this->heap_node_vec.back().get();
                    this->push_up_at(this->heap_node_vec.size() - 1u);
                    this->auto_evict_memory_sz += mem_view.size();

                    try
                    {
                        auto [new_map_ptr, _] = this->reference_map.insert({reference_addr, heap_addr});
                        map_ptr = new_map_ptr;
                    }
                    catch (...)
                    {
                        std::abort();
                    }
                }

                map_ptr->second->reference_counter  += 1;
                map_ptr->second->last_updated       = std::chrono::system_clock::now();

                this->update_heap_node(map_ptr->second);

                return MemoryReference
                {
                    .device_ptr     = map_ptr->second->cuda_mem_ptr.get(),
                    .ptr_mem_sz     = map_ptr->second->cuda_mem_ptr_sz,
                    .internal_key   = reference_addr
                };
            }

            void release_memory(const MemoryReference& memory_reference) noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                auto map_ptr = this->reference_map.find(memory_reference.internal_key);

                if (map_ptr == this->reference_map.end())
                {
                    std::abort();
                }

                if (map_ptr->second->reference_counter == 0u)
                {
                    std::abort();
                }

                map_ptr->second->reference_counter  -= 1;
                map_ptr->second->last_updated       = std::chrono::system_clock::now();

                this->update_heap_node(map_ptr->second);
            }

            auto evict_memory(const std::shared_ptr<void>& immutable_reference) noexcept -> bool
            {
                if (immutable_reference.get() == nullptr)
                {
                    std::abort();
                }

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                uintptr_t reference_addr    = reinterpret_cast<uintptr_t>(immutable_reference.get());
                return this->internal_evict_memory(reference_addr);
            }

        private:

            static void swap_heap_node(std::unique_ptr<HeapNode>& lhs, std::unique_ptr<HeapNode>& rhs) noexcept
            {
                std::swap(lhs, rhs);
                std::swap(lhs->heap_idx, rhs->heap_idx);
            }

            auto internal_evict_memory(uintptr_t reference_addr) noexcept -> bool
            {
                auto map_ptr                = this->reference_map.find(reference_addr);

                if (map_ptr == this->reference_map.end())
                {
                    return true;
                }

                if (map_ptr->second->reference_counter != 0u)
                {
                    return false;
                }

                size_t positional_idx       = map_ptr->second->heap_idx;
                size_t evicting_sz          = map_ptr->second->cuda_mem_ptr_sz;

                this->reference_map.erase(map_ptr);
                this->auto_evict_memory_sz  -= evicting_sz;

                this->swap_heap_node(this->heap_node_vec[positional_idx], this->heap_node_vec.back());
                this->heap_node_vec.pop_back();

                if (positional_idx < this->heap_node_vec.size())
                {
                    this->correct_at(positional_idx);
                }

                return true;
            }

            void auto_evict() noexcept
            {
                while (true)
                {
                    if (this->auto_evict_memory_sz < this->auto_evict_memory_threshold)
                    {
                        return;
                    }

                    if (this->heap_node_vec.empty())
                    {
                        return;
                    }

                    if (this->heap_node_vec.front()->reference_counter != 0u)
                    {
                        return;
                    }

                    bool evict_status = this->internal_evict_memory(this->heap_node_vec.front()->reverse_reference);

                    if (!evict_status)
                    {
                        std::abort();
                    }
                }
            }

            auto is_better(const std::unique_ptr<HeapNode>& lhs, const std::unique_ptr<HeapNode>& rhs) noexcept -> bool
            {
                if (lhs == nullptr)
                {
                    std::abort();
                }
                
                if (rhs == nullptr)
                {
                    std::abort();
                }

                if (lhs->reference_counter < rhs->reference_counter)
                {
                    return true;
                }

                if (lhs->reference_counter > rhs->reference_counter)
                {
                    return false;
                }

                if (lhs->last_updated < rhs->last_updated)
                {
                    return true;
                }

                if (lhs->last_updated > rhs->last_updated)
                {
                    return false;
                }

                return false;
            }

            void push_up_at(size_t idx) noexcept
            {
                if (idx >= this->heap_node_vec.size())
                {
                    std::abort();
                }

                if (idx == 0u)
                {
                    return;
                }

                size_t parent_idx = (idx - 1) / 2;

                if (this->is_better(this->heap_node_vec[parent_idx], this->heap_node_vec[idx]))
                {
                    return;
                }

                this->swap_heap_node(this->heap_node_vec[parent_idx], this->heap_node_vec[idx]);
                this->push_up_at(parent_idx);
            }

            void push_down_at(size_t idx) noexcept
            {
                if (idx >= this->heap_node_vec.size())
                {
                    std::abort();
                }

                size_t cand = idx * 2 + 1;

                if (cand >= this->heap_node_vec.size())
                {
                    return;
                }

                if (cand + 1 < this->heap_node_vec.size() && this->is_better(this->heap_node_vec[cand + 1], this->heap_node_vec[cand]))
                {
                    cand += 1;
                }

                if (this->is_better(this->heap_node_vec[idx], this->heap_node_vec[cand]))
                {
                    return;
                }

                this->swap_heap_node(this->heap_node_vec[idx], this->heap_node_vec[cand]);
                this->push_down_at(cand);
            }

            void correct_at(size_t idx) noexcept
            {
                this->push_up_at(idx);
                this->push_down_at(idx);
            }

            void update_heap_node(HeapNode * heap_node) noexcept
            {
                this->correct_at(heap_node->heap_idx);
            }
    };

    class DistributedCudaImmutableMemoryCache: public virtual ImmutableMemoryCacheInterface
    {
        private:

            std::vector<std::unique_ptr<CudaImmutableMemoryCache>> base_vec;
        
        public:

            DistributedCudaImmutableMemoryCache(std::shared_ptr<MemoryAllocatorInterface> allocator,
                                                size_t auto_evict_memory_threshold,
                                                size_t concurrency_sz)
            {
                if (concurrency_sz == 0u)
                {
                    throw std::invalid_argument("bad concurrency size, 0");
                }

                this->base_vec      = std::vector<std::unique_ptr<CudaImmutableMemoryCache>>(concurrency_sz);

                for (size_t i = 0u; i < concurrency_sz; ++i)
                {
                    this->base_vec[i] = std::make_unique<CudaImmutableMemoryCache>(allocator, auto_evict_memory_threshold);
                }
            }

            auto acquire_memory(const std::shared_ptr<void>& immutable_reference) noexcept -> std::optional<MemoryReference>
            {
                size_t idx = reinterpret_cast<uintptr_t>(immutable_reference.get()) % this->base_vec.size();

                return this->base_vec[idx]->acquire_memory(immutable_reference);
            }

            auto cache_n_acquire_memory(const std::shared_ptr<void>& immutable_reference, std::string_view mem) -> MemoryReference
            {
                size_t idx = reinterpret_cast<uintptr_t>(immutable_reference.get()) % this->base_vec.size();

                return this->base_vec[idx]->cache_n_acquire_memory(immutable_reference, mem);
            }

            void release_memory(const MemoryReference& memory_reference) noexcept
            {
                size_t idx = memory_reference.internal_key % this->base_vec.size();

                this->base_vec[idx]->release_memory(memory_reference);
            }

            auto evict_memory(const std::shared_ptr<void>& immutable_reference) noexcept -> bool
            {
                size_t idx = reinterpret_cast<uintptr_t>(immutable_reference.get()) % this->base_vec.size();

                return this->base_vec[idx]->evict_memory(immutable_reference);
            }
    };

    class MemoryLifetimeManager: public virtual MemoryLifetimeManagerInterface
    {
        private:

            struct LifetimeBucket
            {
                std::shared_ptr<void> immutable_reference;
                std::chrono::time_point<std::chrono::system_clock> expiry;
            };

            std::vector<std::unique_ptr<LifetimeBucket>> lifetime_bucket_vec;
            std::unordered_map<uintptr_t, LifetimeBucket *> reverse_map;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

            auto get_lifetime_bucket_comparator()
            {
                auto cmp_func = [](const std::unique_ptr<LifetimeBucket>& lhs, const std::unique_ptr<LifetimeBucket>& rhs)
                {
                    return lhs->expiry <= rhs->expiry;
                };

                return cmp_func;
            }

        public:
            
            MemoryLifetimeManager(): lifetime_bucket_vec(),
                                     reverse_map(),
                                     mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void extend_lifetime(const std::shared_ptr<void>& immutable_reference, std::chrono::nanoseconds lifetime)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                uintptr_t reference_addr    = reinterpret_cast<uintptr_t>(immutable_reference.get());
                auto map_ptr                = this->reverse_map.find(reference_addr);
                auto expiry                 = std::chrono::time_point_cast<typename std::chrono::time_point<std::chrono::system_clock>::duration>(std::chrono::system_clock::now() + lifetime);

                if (map_ptr == this->reverse_map.end())
                {
                    this->lifetime_bucket_vec.push_back
                    (
                        std::make_unique<LifetimeBucket>
                        (
                            LifetimeBucket
                            {
                                .immutable_reference = immutable_reference,
                                .expiry = expiry
                            }
                        )
                    );

                    LifetimeBucket * bucket_reference = this->lifetime_bucket_vec.back().get();
                    algorithm_extension::push_heap(this->lifetime_bucket_vec.begin(), this->lifetime_bucket_vec.end(), this->get_lifetime_bucket_comparator());

                    try
                    {
                        auto [new_map_ptr, _] = this->reverse_map.insert({reference_addr, bucket_reference});
                        map_ptr = new_map_ptr;
                    }
                    catch (...)
                    {
                        std::abort();
                    }
                }

                map_ptr->second->expiry = std::max(expiry, map_ptr->second->expiry);
            }

            auto get_expired_memory_vector() -> std::vector<std::shared_ptr<void>>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                std::vector<std::shared_ptr<void>> result{};
                std::chrono::time_point<std::chrono::system_clock> bar = std::chrono::system_clock::now();

                try
                {
                    while (true)
                    {
                        if (this->lifetime_bucket_vec.empty())
                        {
                            return result;
                        }

                        if (this->lifetime_bucket_vec.front()->expiry >= bar)
                        {
                            return result;
                        }

                        uintptr_t reference_addr = reinterpret_cast<uintptr_t>(this->lifetime_bucket_vec.front()->immutable_reference.get());
                        this->reverse_map.erase(reference_addr);

                        algorithm_extension::pop_heap(this->lifetime_bucket_vec.begin(), this->lifetime_bucket_vec.end(), this->get_lifetime_bucket_comparator());
                        auto loose_bucket = std::move(this->lifetime_bucket_vec.back());
                        this->lifetime_bucket_vec.pop_back();
                        result.push_back(std::move(loose_bucket->immutable_reference));
                    }
                }
                catch (...)
                {
                    std::abort();
                }

                return result;
            }
    };

    class DistributedMemoryLifetimeManager: public virtual MemoryLifetimeManagerInterface
    {
        private:

            using randomizer_t = decltype(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(0u)}));

            std::vector<std::unique_ptr<MemoryLifetimeManagerInterface>> base_vec;
            randomizer_t randomizer;

        public:

            DistributedMemoryLifetimeManager(std::vector<std::unique_ptr<MemoryLifetimeManagerInterface>>&& base_vec): randomizer(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())}))
            {
                if (base_vec.empty())
                {
                    throw std::invalid_argument("bad base vector, 0");
                }

                this->base_vec = std::move(base_vec);
            }
                                                
            void extend_lifetime(const std::shared_ptr<void>& immutable_reference, std::chrono::nanoseconds lifetime)
            {
                if (this->base_vec.size() == 0u)
                {
                    std::abort();
                }

                size_t idx = reinterpret_cast<uintptr_t>(immutable_reference.get()) % this->base_vec.size();
                this->base_vec[idx]->extend_lifetime(immutable_reference, lifetime);
            }

            auto get_expired_memory_vector() -> std::vector<std::shared_ptr<void>>
            {
                size_t seed = this->randomizer();
                std::vector<std::shared_ptr<void>> result{};

                for (size_t i = 0u; i < this->base_vec.size(); ++i)
                {
                    size_t offset = (seed + i) % this->base_vec.size();

                    try
                    {
                        auto tmp = this->base_vec[i]->get_expired_memory_vector();
                        std::copy(std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()), std::back_inserter(result));
                    }
                    catch (...)
                    {
                        std::abort();
                    }
                }

                return result;
            }
    };

    class EvictionWorker: public virtual cron_subsystem::UpdatableInterface
    {
        private:

            std::shared_ptr<ImmutableMemoryCacheInterface> memory_cache;
            std::shared_ptr<MemoryLifetimeManagerInterface> memory_manager;
            std::chrono::nanoseconds reentrant_lifetime;

        public:

            EvictionWorker(std::shared_ptr<ImmutableMemoryCacheInterface> memory_cache,
                           std::shared_ptr<MemoryLifetimeManagerInterface> memory_manager,
                           std::chrono::nanoseconds reentrant_lifetime) noexcept: memory_cache(std::move(memory_cache)),
                                                                                  memory_manager(std::move(memory_manager)),
                                                                                  reentrant_lifetime(reentrant_lifetime){}

            void update()
            {
                try
                {
                    std::vector<std::shared_ptr<void>> evictable_vec = this->memory_manager->get_expired_memory_vector();

                    for (const auto& evictable: evictable_vec)
                    {
                        bool was_evicted = this->memory_cache->evict_memory(evictable);

                        if (!was_evicted)
                        {
                            this->memory_manager->extend_lifetime(evictable, this->reentrant_lifetime);
                        }
                    }
                }   
                catch (...)
                {
                    logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("immutable_multiplatform_memory")
                                                                                   .topic("EvictionWorker")
                                                                                   .error()
                                                                                   .message(std::current_exception())
                                                                                   .get());
                }             
            }
    };

    class SelfManagedExternalImmutableMemoryCache: public virtual ExternalImmutableMemoryCacheInterface
    {
        private:

            std::shared_ptr<MemoryLifetimeManagerInterface> memory_lifetime_manager;
            std::shared_ptr<ImmutableMemoryCacheInterface> base;
            std::shared_ptr<void> daemon_worker;
            std::chrono::nanoseconds lifetime;

        public:

            SelfManagedExternalImmutableMemoryCache(std::shared_ptr<MemoryLifetimeManagerInterface> memory_lifetime_manager,
                                                    std::shared_ptr<ImmutableMemoryCacheInterface> base,
                                                    std::shared_ptr<void> daemon_worker,
                                                    std::chrono::nanoseconds lifetime) noexcept: memory_lifetime_manager(std::move(memory_lifetime_manager)),
                                                                                                 base(std::move(base)),
                                                                                                 daemon_worker(std::move(daemon_worker)),
                                                                                                 lifetime(lifetime){}

            auto acquire_memory(const std::shared_ptr<void>& immutable_reference) noexcept -> std::optional<MemoryReference>
            {
                if (immutable_reference == nullptr)
                {
                    std::abort();
                }

                std::optional<MemoryReference> rs = this->base->acquire_memory(immutable_reference);

                try
                {
                    this->memory_lifetime_manager->extend_lifetime(immutable_reference, this->lifetime);
                }
                catch (...)
                {
                    logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("immutable_multiplatform_memory")
                                                                                   .topic("SelfManagedExternalImmutableMemoryCache")
                                                                                   .error()
                                                                                   .message(std::current_exception())
                                                                                   .get());
                }

                return rs;
            }

            auto cache_n_acquire_memory(const std::shared_ptr<void>& immutable_reference, std::string_view mem_view) -> MemoryReference
            {
                if (immutable_reference == nullptr)
                {
                    std::abort();
                }

                MemoryReference rs = this->base->cache_n_acquire_memory(immutable_reference, mem_view);

                try
                {
                    this->memory_lifetime_manager->extend_lifetime(immutable_reference, this->lifetime);
                }
                catch (...)
                {
                    logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("immutable_multiplatform_memory")
                                                                                   .topic("SelfManagedExternalImmutableMemoryCache")
                                                                                   .error()
                                                                                   .message(std::current_exception())
                                                                                   .get());
                }

                return rs;
            }

            void release_memory(const MemoryReference& memory_reference) noexcept
            {
                this->base->release_memory(memory_reference);                
            }
    };

    class Factory
    {
        private:

            static auto get_normal_lifetime_manager(size_t concurrent_sz) -> std::unique_ptr<MemoryLifetimeManagerInterface>
            {
                std::vector<std::unique_ptr<MemoryLifetimeManagerInterface>> base_vec{};

                for (size_t i = 0u; i < concurrent_sz; ++i)
                {
                    base_vec.push_back(std::make_unique<MemoryLifetimeManager>());
                }

                return std::make_unique<DistributedMemoryLifetimeManager>(std::move(base_vec));
            }

            static auto get_normal_internal_immutable_memory_cache(const std::shared_ptr<MemoryAllocatorInterface>& allocator,
                                                                   size_t auto_evict_byte_sz,
                                                                   size_t concurrent_sz) -> std::unique_ptr<ImmutableMemoryCacheInterface>
            {
                return std::make_unique<DistributedCudaImmutableMemoryCache>(allocator, auto_evict_byte_sz, concurrent_sz);
            }

            static auto get_normal_daemon_worker(const std::shared_ptr<MemoryLifetimeManagerInterface>& lifetime_manager,
                                                 const std::shared_ptr<ImmutableMemoryCacheInterface>& memory_cache,
                                                 std::chrono::nanoseconds memory_lifetime,
                                                 std::chrono::nanoseconds cron_periodic_dur) -> std::shared_ptr<void>
            {
                const std::chrono::nanoseconds MIN_MEMORY_LIFETIME      = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1));
                const std::chrono::nanoseconds MAX_MEMORY_LIFETIME      = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));
                const std::chrono::nanoseconds MIN_CRON_PERIODIC_DUR    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1));
                const std::chrono::nanoseconds MAX_CRON_PERIODIC_DUR    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));

                if (lifetime_manager == nullptr)
                {
                    throw std::invalid_argument("bad lifetime manager, null");
                }

                if (memory_cache == nullptr)
                {
                    throw std::invalid_argument("bad memory_cache, null");
                }

                memory_lifetime     = std::clamp(memory_lifetime, MIN_MEMORY_LIFETIME, MAX_MEMORY_LIFETIME);
                cron_periodic_dur   = std::clamp(cron_periodic_dur, MIN_CRON_PERIODIC_DUR, MAX_CRON_PERIODIC_DUR);

                return cron_subsystem::register_periodic_cronjob(std::make_unique<EvictionWorker>(memory_cache, lifetime_manager, memory_lifetime),
                                                                 cron_periodic_dur);
            }

        public:

            static auto get_normal_immutable_memory_cache(std::chrono::nanoseconds memory_lifetime,
                                                          std::chrono::nanoseconds cron_periodic_dur,
                                                          size_t auto_evict_byte_sz,
                                                          size_t concurrent_sz,
                                                          const std::shared_ptr<MemoryAllocatorInterface>& allocator) -> std::unique_ptr<ExternalImmutableMemoryCacheInterface>
            {
                const std::chrono::nanoseconds MIN_LIFETIME = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1));
                const std::chrono::nanoseconds MAX_LIFETIME = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));

                memory_lifetime                                                     = std::clamp(memory_lifetime, MIN_LIFETIME, MAX_LIFETIME);
                std::shared_ptr<MemoryLifetimeManagerInterface> lifetime_manager    = get_normal_lifetime_manager(concurrent_sz);
                std::shared_ptr<ImmutableMemoryCacheInterface> memory_cache         = get_normal_internal_immutable_memory_cache(allocator, auto_evict_byte_sz, concurrent_sz);
                std::shared_ptr<void> daemon_worker                                 = get_normal_daemon_worker(lifetime_manager, memory_cache, memory_lifetime, cron_periodic_dur);

                return std::make_unique<SelfManagedExternalImmutableMemoryCache>(std::move(lifetime_manager),
                                                                                 std::move(memory_cache),
                                                                                 std::move(daemon_worker),
                                                                                 memory_lifetime);
            }
    };
}

#endif