#ifndef __CUDA_MANAGEMENT_SCOPE_ALLOCATOR_H__
#define __CUDA_MANAGEMENT_SCOPE_ALLOCATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include "cuda_vector.h"
#include "device_memory.h"
#include <type_traits>
#include <optional>

namespace cuda_management::scope_allocator
{
    template <class ScopeInterface>
    class scope_guard
    {
        private:

            ScopeInterface * volatile base;

            using self = scope_guard;

        public:

            __device__ inline __attribute__((always_inline)) scope_guard(ScopeInterface * base): base(utility::safe_ptr_access(base))
            {
                this->base->in_scope();
            }

            __device__ scope_guard(const self&) = delete;
            __device__ scope_guard(self&&) = delete;

            __device__ self& operator =(const self&) = delete;
            __device__ self& operator =(self&&) = delete;

            __device__ inline __attribute__((always_inline)) ~scope_guard() noexcept
            {
                this->base->out_scope();
            }
    };

    template <class Allocator = cuda_management::device_memory::CudaAllocator>
    class SplitStackAllocator
    {
        private:

            struct MemorySegment
            {
                char * buf;
                size_t buf_sz;
            };

            struct MemoryPoint
            {
                size_t slot;
                size_t offset;
            };

            struct SavedMemoryPoint
            {
                bool is_optional;
                MemoryPoint value;
            };

            cuda_vector::trivial_cuda_vector<MemorySegment, Allocator> stack_buffer_vec;
            cuda_vector::trivial_cuda_vector<SavedMemoryPoint, Allocator> saved_point_vec;
            std::optional<MemoryPoint> valid_point;
            size_t base_sz;

        public:

            __device__ SplitStackAllocator(size_t base_sz): stack_buffer_vec(),
                                                            saved_point_vec(),
                                                            valid_point(std::nullopt),
                                                            base_sz(base_sz){}

            __device__ SplitStackAllocator(): SplitStackAllocator(1024u){}

            __device__ ~SplitStackAllocator() noexcept
            {
                for (size_t i = 0u; i < stack_buffer_vec.size(); ++i)
                {
                    Allocator{}.free(stack_buffer_vec[i].buf);
                }
            }

            __device__ inline void in_scope()
            {
                if (this->valid_point.has_value()) [[likely]]
                {
                    this->saved_point_vec.push_back(this->to_saved_memory_point(this->valid_point));
                }
                else [[unlikely]]
                {
                    return slow_enter_scope();
                }
            }

            __device__ inline auto malloc(size_t blk_sz) -> void *
            {
                if (this->valid_point.has_value() && this->stack_buffer_vec[this->valid_point->slot].buf_sz - this->valid_point->offset >= blk_sz) [[likley]]
                {
                    void * rs           = std::next(this->stack_buffer_vec[this->valid_point->slot].buf, this->valid_point->offset); 
                    this->valid_point   = MemoryPoint
                    {
                        .slot   = this->valid_point->slot,
                        .offset = this->valid_point->offset + blk_sz
                    };

                    return rs;
                }
                else [[unlikely]]
                {
                    return this->slow_allocate(blk_sz);
                }
            }

            __device__ inline void free(void * buf) noexcept
            {
                (void) buf;
            }

            __device__ inline void out_scope() noexcept
            {
                if (this->saved_point_vec.empty())
                {
                    assert(false);
                }

                this->valid_point = this->to_memory_point(this->saved_point_vec.back());
                this->saved_point_vec.pop_back();
            }

        private:

            __device__ constexpr auto to_saved_memory_point(const std::optional<MemoryPoint>& arg) -> SavedMemoryPoint
            {
                if (!arg.has_value())
                {
                    return 
                    {
                        .is_optional    = true,
                        .value          = {}
                    };
                }

                return
                {
                    .is_optional    = false,
                    .value          = arg.value()
                };
            }

            __device__ constexpr auto to_memory_point(const SavedMemoryPoint& arg) -> std::optional<MemoryPoint>
            {
                if (arg.is_optional)
                {
                    return std::nullopt;
                }

                return arg.value;
            }

            __device__ __attribute__((noinline)) void slow_enter_scope()
            {
                MemoryPoint next_point;

                if (this->valid_point.has_value())
                {
                    next_point = this->valid_point.value();
                }
                else
                {
                    next_point = MemoryPoint
                    {
                        .slot   = 0u,
                        .offset = 0u
                    };

                    this->reserve_for_vector_size_of(1);
                }

                this->saved_point_vec.push_back(this->to_saved_memory_point(this->valid_point));
                this->valid_point = next_point;
            }

            __device__ __attribute__((noinline)) auto slow_allocate(size_t blk_sz) noexcept -> void *
            {
                if (!this->valid_point.has_value())
                {
                    assert(false);
                }

                MemoryPoint cur_point = this->valid_point.value();

                while (true)
                {
                    size_t point_sz = this->stack_buffer_vec[cur_point.slot].buf_sz - cur_point.offset;

                    if (point_sz >= blk_sz)
                    {
                        void * rs   = std::next(this->stack_buffer_vec[cur_point.slot].buf, cur_point.offset); 
                        cur_point   = MemoryPoint
                        {
                            .slot   = cur_point.slot,
                            .offset = cur_point.offset + blk_sz
                        };

                        this->valid_point   = cur_point;

                        return rs;
                    }

                    this->reserve_for_vector_size_of(cur_point.slot + 2u);

                    cur_point = MemoryPoint
                    {
                        .slot   = cur_point.slot + 1u,
                        .offset = 0u
                    };
                }
            }

            __device__ void reserve_for_vector_size_of(size_t sz)
            {
                size_t new_sz   = std::max(sz, static_cast<size_t>(this->stack_buffer_vec.size()));
                size_t old_sz   = this->stack_buffer_vec.size();
                size_t diff_sz  = new_sz - old_sz; 

                for (size_t i = 0u; i < diff_sz; ++i)
                {
                    size_t offset       = old_sz + i; 
                    size_t buf_vec_sz   = this->base_sz * (size_t{1} << offset);
                    char * new_buf      = static_cast<char *>(Allocator().malloc(buf_vec_sz));

                    this->stack_buffer_vec.push_back(MemorySegment
                    {
                        .buf    = new_buf,
                        .buf_sz = buf_vec_sz
                    });
                }
            }            
    };
}

#endif