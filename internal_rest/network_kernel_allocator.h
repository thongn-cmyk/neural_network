#ifndef __DG_NETWORK_KERNEL_ALLOCATOR__
#define __DG_NETWORK_KERNEL_ALLOCATOR__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <thread>
#include <mutex>
#include "stdx.h"
#include "network_exception.h"
#include "network_log.h"
#include "network_concurrency.h"

namespace dg_sock::network_kernel_allocator
{
    class BatchAllocatorInterface
    {
        public:

            virtual ~BatchAllocatorInterface() noexcept = default;

            virtual void malloc(std::add_pointer_t<void> * ret_mem_arr, size_t request_mem_blk_count, exception_t * exception_arr) noexcept = 0;
            virtual void free(std::add_pointer_t<void> * free_mem_arr, size_t arr_sz) noexcept = 0;
            virtual auto malloc_size() const noexcept -> size_t = 0;
            virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    class BaseAllocatorInterface
    {
        public:

            virtual ~BaseAllocatorInterface() noexcept = default;

            virtual auto malloc() noexcept -> std::expected<void *, exception_t> = 0;
            virtual void free(void *) noexcept = 0;
            virtual auto malloc_size() const noexcept -> size_t = 0;
    };

    template <class T>
    class AllocatorInterface: public virtual BaseAllocatorInterface{};

    class BatchAllocator : public virtual BatchAllocatorInterface
    {
        private:

            std::vector<std::shared_ptr<void>> memreference_queue;
            std::vector<void *> mem_queue;
            size_t mem_queue_cap;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;
            stdxx::hdi_container<size_t> mempiece_sz;
            stdxx::hdi_container<size_t> max_consume_per_load;

        public:

            BatchAllocator(size_t mem_queue_cap,
                           size_t mempiece_sz,
                           size_t max_consume_per_load): memreference_queue(),
                                                         mem_queue(),
                                                         mem_queue_cap(mem_queue_cap),
                                                         mtx(stdxx::make_unique_fair_atomic_flag()),
                                                         mempiece_sz(stdxx::hdi_container<size_t>{mempiece_sz}),
                                                         max_consume_per_load(stdxx::hdi_container<size_t>{max_consume_per_load}){}

            void malloc(std::add_pointer_t<void> * ret_mem_arr, size_t request_mem_blk_count, exception_t * exception_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (request_mem_blk_count > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                size_t allocatable_sz   = this->mem_queue_cap - this->memreference_queue.size();
                size_t missing_sz       = request_mem_blk_count - std::min(request_mem_blk_count, this->mem_queue.size());
                size_t doable_sz        = std::min(allocatable_sz, missing_sz);

                this->noexcept_allocate(doable_sz);

                size_t fulfillable_sz   = std::min(static_cast<size_t>(this->mem_queue.size()), request_mem_blk_count);
                size_t rem_sz           = this->mem_queue.size() - fulfillable_sz;   

                std::copy(std::next(this->mem_queue.begin(), rem_sz), this->mem_queue.end(), ret_mem_arr);
                this->mem_queue.resize(rem_sz);

                std::fill(exception_arr, std::next(exception_arr, fulfillable_sz), dg_sock::network_exception::SUCCESS);
                std::fill(std::next(exception_arr, fulfillable_sz), std::next(exception_arr, request_mem_blk_count), dg_sock::network_exception::RESOURCE_EXHAUSTION);
            }

            void free(std::add_pointer_t<void> * free_mem_arr, size_t arr_sz) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                size_t old_sz   = this->mem_queue.size();
                size_t new_sz   = old_sz + arr_sz;

                this->mem_queue.resize(new_sz);
                std::copy(free_mem_arr, std::next(free_mem_arr, arr_sz), std::next(this->mem_queue.begin(), old_sz));
            }

            auto malloc_size() const noexcept -> size_t
            {
                return this->mempiece_sz.value;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load.value;
            } 

        private:

            void noexcept_allocate(size_t allocatable_sz) noexcept
            {
                for (size_t i = 0u; i < allocatable_sz; ++i)
                {
                    try
                    {
                        size_t promoted_sz                  = stdxx::mul_ceil(this->mempiece_sz.value, alignof(std::max_align_t));
                        std::shared_ptr<void> memreference  = std::unique_ptr<void, decltype(&std::free)>(std::aligned_alloc(alignof(std::max_align_t), promoted_sz), std::free);

                        this->memreference_queue.push_back(memreference);
                        this->mem_queue.push_back(memreference.get());
                    }
                    catch (...)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }
            }
    };

    class AffinedAllocator : public virtual AllocatorInterface<AffinedAllocator>
    {
        private:

            std::shared_ptr<BatchAllocatorInterface> base_allocator;
            size_t refill_sz;
            std::vector<void *> mem_vec;
            size_t mem_vec_capacity;
            std::vector<void *> free_vec;
            size_t free_vec_capacity;

        public:

            AffinedAllocator(std::shared_ptr<BatchAllocatorInterface> base_allocator,
                             size_t refill_sz,
                             std::vector<void *> mem_vec,
                             size_t mem_vec_capacity,
                             std::vector<void *> free_vec,
                             size_t free_vec_capacity): base_allocator(std::move(base_allocator)),
                                                        refill_sz(refill_sz),
                                                        mem_vec(std::move(mem_vec)),
                                                        mem_vec_capacity(mem_vec_capacity),
                                                        free_vec(std::move(free_vec)),
                                                        free_vec_capacity(free_vec_capacity){}

            auto malloc() noexcept -> std::expected<void *, exception_t>
            {
                if (this->mem_vec.empty())
                {
                    exception_t err = this->refill_mem_vec();

                    if (dg_sock::network_exception::is_failed(err))
                    {
                        return std::unexpected(dg_sock::network_exception::RESOURCE_EXHAUSTION);
                    }
                }

                void * rs = this->mem_vec.back();
                this->mem_vec.pop_back();

                return rs;
            }

            void free(void * mem_ptr) noexcept
            {
                if (this->mem_vec.size() == this->mem_vec_capacity)
                {
                    if (this->free_vec.size() == this->free_vec_capacity)
                    {
                        this->empty_free_vec();
                    }

                    this->free_vec.push_back(mem_ptr);
                    return;
                }

                this->mem_vec.push_back(mem_ptr);
            }

            auto malloc_size() const noexcept -> size_t
            {
                return this->base_allocator->malloc_size();
            }

        private:

            auto refill_mem_vec() noexcept -> exception_t
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (!this->mem_vec.empty())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (!this->free_vec.empty())
                {
                    size_t move_sz  = std::min(static_cast<size_t>(this->free_vec.size()), this->mem_vec_capacity);
                    size_t rem_sz   = this->free_vec.size() - move_sz;

                    std::copy(std::next(this->free_vec.begin(), rem_sz), this->free_vec.end(), std::back_inserter(this->mem_vec));
                    this->free_vec.resize(rem_sz);

                    return dg_sock::network_exception::SUCCESS;
                }

                dg_sock::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<void>[]> mem_arr(this->refill_sz);
                dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(this->refill_sz);

                this->base_allocator->malloc(mem_arr.get(), this->refill_sz, exception_arr.get());

                for (size_t i = 0u; i < this->refill_sz; ++i)
                {
                    if (dg_sock::network_exception::is_success(exception_arr[i]))
                    {
                        this->mem_vec.push_back(mem_arr[i]);
                    }
                }

                if (this->mem_vec.empty())
                {
                    return dg_sock::network_exception::RESOURCE_EXHAUSTION;
                }

                return dg_sock::network_exception::SUCCESS;
            }

            void empty_free_vec() noexcept
            {
                if (this->free_vec.empty())
                {
                    return;
                }

                this->base_allocator->free(this->free_vec.data(), this->free_vec.size());
                this->free_vec.clear();
            }
    };

    class AffinedMapAllocator : public virtual AllocatorInterface<AffinedMapAllocator>
    {
        private:

            std::vector<std::unique_ptr<AllocatorInterface<AffinedAllocator>>> affined_map;
            size_t mempiece_sz;

        public:

            AffinedMapAllocator(std::vector<std::unique_ptr<AllocatorInterface<AffinedAllocator>>> affined_map,
                                size_t mempiece_sz): affined_map(std::move(affined_map)),
                                                     mempiece_sz(mempiece_sz){}

            auto malloc() noexcept -> std::expected<void *, exception_t> 
            {
                size_t thr_idx =  dg_sock::network_concurrency::this_thread_idx();

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (thr_idx >= this->affined_map.size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                return this->affined_map[thr_idx]->malloc();
            }

            void free(void * mem_ptr) noexcept
            {
                size_t thr_idx = dg_sock::network_concurrency::this_thread_idx();

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (thr_idx >= this->affined_map.size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                this->affined_map[thr_idx]->free(mem_ptr);
            }

            auto malloc_size() const noexcept -> size_t
            {
                return this->mempiece_sz;
            }
    };

    struct ComponentFactory
    {
        static auto make_batch_allocator(size_t mempiece_count,
                                         size_t mempiece_sz,
                                         size_t consume_decay_factor = 4u) -> std::unique_ptr<BatchAllocatorInterface>
        {
            const size_t MIN_MEMPIECE_COUNT = 1u;
            const size_t MAX_MEMPIECE_COUNT = size_t{1} << 30;
            const size_t MIN_MEMPIECE_SZ    = 1u;
            const size_t MAX_MEMPIECE_SZ    = size_t{1} << 30;

            if (std::clamp(mempiece_count, MIN_MEMPIECE_COUNT, MAX_MEMPIECE_COUNT) != mempiece_count)
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(mempiece_sz, MIN_MEMPIECE_SZ, MAX_MEMPIECE_SZ) != mempiece_sz)
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_consume_sz = mempiece_count >> consume_decay_factor;
            size_t consume_sz           = std::max(size_t{1}, tentative_consume_sz); 

            return std::make_unique<BatchAllocator>(mempiece_count,
                                                    mempiece_sz,
                                                    consume_sz);
        }

        static auto make_affined_allocator(std::shared_ptr<BatchAllocatorInterface> base_allocator,
                                           size_t refill_sz,
                                           size_t mem_vec_capacity,
                                           size_t free_vec_capacity) -> std::unique_ptr<AllocatorInterface<AffinedAllocator>>
        {
            const size_t MIN_REFILL_SZ          = 1u;
            const size_t MAX_REFILL_SZ          = size_t{1} << 30;
            const size_t MIN_MEM_VEC_CAPACITY   = 1u;
            const size_t MAX_MEM_VEC_CAPACITY   = size_t{1} << 30;
            const size_t MIN_FREE_VEC_CAPACITY  = 1u;
            const size_t MAX_FREE_VEC_CAPACITY  = size_t{1} << 30;

            if (base_allocator == nullptr)
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(refill_sz, MIN_REFILL_SZ, MAX_REFILL_SZ) != refill_sz)
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(mem_vec_capacity, MIN_MEM_VEC_CAPACITY, MAX_MEM_VEC_CAPACITY) != mem_vec_capacity)
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(free_vec_capacity, MIN_FREE_VEC_CAPACITY, MAX_FREE_VEC_CAPACITY) != free_vec_capacity)
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            size_t adjusted_refill_sz = std::min(refill_sz, static_cast<size_t>(base_allocator->max_consume_size())); 

            return std::make_unique<AffinedAllocator>(base_allocator,
                                                      adjusted_refill_sz,
                                                      std::vector<void *>(),
                                                      mem_vec_capacity,
                                                      std::vector<void *>(),
                                                      free_vec_capacity);
        }

        static auto make_affined_map_allocator(std::shared_ptr<BatchAllocatorInterface> base_allocator,
                                               size_t affined_refill_sz,
                                               size_t affined_mem_vec_capacity,
                                               size_t affined_free_vec_capacity) -> std::unique_ptr<AllocatorInterface<AffinedMapAllocator>>
        {
            std::vector<std::unique_ptr<AllocatorInterface<AffinedAllocator>>> affined_map{};

            for (size_t i = 0u; i < dg_sock::network_concurrency::MAX_THREAD_COUNT; ++i)
            {
                affined_map.push_back(make_affined_allocator(base_allocator,
                                                             affined_refill_sz,
                                                             affined_mem_vec_capacity,
                                                             affined_free_vec_capacity));
            }

            return std::make_unique<AffinedMapAllocator>(std::move(affined_map), base_allocator->malloc_size());
        }
    };
}

#endif