#ifndef __CUDA_MANAGEMENT_CUDA_MALLOC_DEDICATED_ALLOCATOR_H__
#define __CUDA_MANAGEMENT_CUDA_MALLOC_DEDICATED_ALLOCATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <memory>
#include <cuda_runtime.h>
#include <cstring>
#include <string_view>
#include <exception>
#include <stdexcept>
#include "assert.h"
#include <cuda_management/local_exception.h>
#include "segment_allocator.h"
#include <variant>
#include <stl_extension/stdx.h>
#include <mutex_extension/fair_mutex.h>
#include <stl_extension/unordered_node_map.h>

namespace cuda_management::cuda_malloc
{
    struct DedicatedAllocatorConfig
    {
        uint64_t heap_memory_sz;
        uint64_t heap_leaf_sz;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(heap_memory_sz, heap_leaf_sz);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(heap_memory_sz, heap_leaf_sz);
        }
    };

    class DedicatedAllocator: public virtual AllocatorInterface
    {
        private:

            struct AllocationMetadata
            {
                std::pair<size_t, size_t> interval;
            };

            static inline constexpr size_t HEAP_OUTDEGREE_SZ    = 8u;

            using BaseAllocatorType = SegmentAllocator<std::integral_constant<size_t, HEAP_OUTDEGREE_SZ>>;

            std::unique_ptr<BaseAllocatorType> allocator;
            std::shared_ptr<char[]> cu_mem;
            size_t leaf_sz;
            unordered_map_variants::unordered_node_map<uintptr_t, AllocationMetadata> allocation_map;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

            static auto get_tree_height_from_leaf_count(size_t leaf_count) -> size_t
            {
                if (leaf_count == 0u)
                {
                    return 0u;
                }

                uint64_t current_base_sz    = 1u;
                size_t current_height       = 1u;
                uint64_t max_base_sz        = uint64_t{1} << 60;

                while (true)
                {
                    if (leaf_count == current_base_sz)
                    {
                        return current_height;
                    }

                    if (leaf_count < current_base_sz)
                    {
                        throw std::invalid_argument("invalid leaf count, leaf count is not HEAP_OUTDEGREE_SZ ^ x");
                    }

                    __uint128_t next_base_sz    = static_cast<__uint128_t>(current_base_sz) * HEAP_OUTDEGREE_SZ;

                    if (next_base_sz > max_base_sz)
                    {
                        throw std::invalid_argument("invalid leaf count, base size out of numeric range");
                    }

                    current_base_sz             = next_base_sz;
                    current_height              += 1;
                }
            }

            static auto get_tree_height(size_t mem_sz, size_t leaf_sz) -> size_t
            {
                if (leaf_sz == 0u)
                {
                    throw std::invalid_argument("bad leaf size, 0");
                }

                if (mem_sz % leaf_sz != 0u)
                {
                    throw std::invalid_argument("bad mem size, mem size is not multiples of leaf size");
                }

                size_t leaf_count   = mem_sz / leaf_sz;

                return get_tree_height_from_leaf_count(leaf_count);
            }

            static auto base_malloc(size_t sz) -> void *
            {
                using namespace cuda_management::local_exception;

                if (sz == 0u)
                {
                    return nullptr;
                }

                void * cuda_buf = nullptr;
                cudaError_t err = cudaMalloc(static_cast<void **>(&cuda_buf), sz);

                if (err != cudaSuccess)
                {
                    throw cuda_bad_alloc();
                }

                if (cuda_buf == nullptr)
                {
                    throw cuda_corruption();
                }

                return cuda_buf;
            }

            static void base_free(void * ptr) noexcept
            {
                using namespace cuda_management::local_exception;

                if (ptr == nullptr)
                {
                    return;
                }

                cudaFree(ptr);
            }

            static auto make_cuda_memory(size_t sz) -> std::shared_ptr<char[]>
            {
                auto destructor = [](char * ptr) noexcept
                {
                    base_free(ptr);
                };
                char * mem      = static_cast<char *>(base_malloc(sz));

                return std::unique_ptr<char[], decltype(destructor)>(mem, std::move(destructor));
            }

            static auto check_leaf_size(size_t sz) -> size_t
            {
                if (sz == 0u)
                {
                    throw std::invalid_argument("bad leaf size, 0");
                }

                return sz;
            }

        public:

            DedicatedAllocator(const DedicatedAllocatorConfig& config): allocator(std::make_unique<BaseAllocatorType>(get_tree_height(config.heap_memory_sz, config.heap_leaf_sz))),
                                                                        cu_mem(make_cuda_memory(config.heap_memory_sz)),
                                                                        leaf_sz(check_leaf_size(config.heap_leaf_sz)),
                                                                        allocation_map(),
                                                                        mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            auto malloc(size_t sz) -> void *
            {
                using namespace cuda_management::local_exception;

                if (sz == 0u)
                {
                    return nullptr;
                }

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                size_t round_sz     = stdx::mul_ceil(sz, this->leaf_sz);
                size_t leaf_count   = round_sz / this->leaf_sz;

                std::optional<std::pair<size_t, size_t>> interval   = this->allocator->malloc(leaf_count);

                if (!interval.has_value())
                {
                    throw cuda_bad_alloc();
                }

                size_t buf_first                = interval->first * this->leaf_sz;
                void * mem                      = std::next(this->cu_mem.get(), buf_first);
                AllocationMetadata  metadata    = AllocationMetadata
                {
                    .interval = interval.value()
                };

                try
                {
                    auto [map_ptr, status] = this->allocation_map.insert(std::make_pair(reinterpret_cast<uintptr_t>(mem), metadata));

                    if (!status)
                    {
                        std::abort();
                    }
                }
                catch (...)
                {
                    std::abort();
                }

                return mem;
            }

            void free(void * ptr) noexcept
            {
                if (ptr == nullptr)
                {
                    return;
                }

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                auto map_ptr = this->allocation_map.find(reinterpret_cast<uintptr_t>(ptr));

                if (map_ptr == this->allocation_map.end())
                {
                    std::abort();
                }

                this->allocator->free(map_ptr->second.interval);
                this->allocation_map.erase(map_ptr);
            }
    };
}

#endif