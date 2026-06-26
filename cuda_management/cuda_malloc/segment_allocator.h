#ifndef __CUDA_MANAGEMENT_CUDA_MALLOC_SEGMENT_ALLOCATOR_H__
#define __CUDA_MANAGEMENT_CUDA_MALLOC_SEGMENT_ALLOCATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <algorithm>
#include <utility>
#include <functional>
#include <optional>
#include <cstdlib>
#include "assert.h"
#include <stl_extension/unordered_node_map.h>
#include <cstring>

namespace cuda_management::cuda_malloc
{
    template <class OutDegreeSize = std::integral_constant<size_t, 2u>, class NodeType = uint64_t>
    class SegmentAllocator
    {
        private:

            struct HeapNode
            {
                NodeType l;
                NodeType r;
                NodeType c;
                NodeType trace;
            };

            std::unique_ptr<HeapNode[]> heap_node_arr;
            size_t heap_node_arr_sz;
            size_t tree_height;

            static inline constexpr NodeType BLOCKED_TRACE          = std::numeric_limits<NodeType>::max();
            static inline constexpr size_t OUT_DEGREE_SIZE_VALUE    = static_cast<size_t>(OutDegreeSize());

        public:

            static_assert(static_cast<size_t>(OutDegreeSize()) >= 2u);
            static_assert(std::is_unsigned_v<NodeType>);

            SegmentAllocator(size_t tree_height)
            {
                std::tie(this->heap_node_arr, this->heap_node_arr_sz) = this->make_heap_node_vec(tree_height);
                this->tree_height = tree_height;
            }

            auto malloc(size_t sz) -> std::optional<std::pair<size_t, size_t>>
            {
                std::optional<std::pair<size_t, size_t>> blk = this->seek_block();

                if (sz == 0u)
                {
                    return std::nullopt;
                }

                if (!blk.has_value())
                {
                    return std::nullopt;
                }

                size_t blk_sz = blk->second;

                if (sz > blk_sz)
                {
                    return std::nullopt;
                }

                this->block_interval({blk->first, sz});

                return std::make_pair(blk->first, sz);
            }

            void free(const std::pair<size_t, size_t>& interval) noexcept
            {
                this->unblock_interval(interval);
            }

            auto base_size() const noexcept -> size_t
            {
                return this->get_base_size();
            }

        private:

            auto get_base_size() const noexcept -> size_t
            {
                return (this->heap_node_arr_sz * (OUT_DEGREE_SIZE_VALUE - 1u) + 1u) / OUT_DEGREE_SIZE_VALUE;
            }

            auto make_trace(size_t offset, bool is_right_trace) const noexcept -> size_t
            {
                return (offset << 1) | static_cast<size_t>(is_right_trace);
            }

            auto read_trace(size_t trace) const noexcept -> std::pair<size_t, bool>
            {
                bool is_right_trace     = static_cast<bool>(trace & size_t{1});
                size_t offset           = trace >> 1;

                return {offset, is_right_trace};
            }

            auto seek_offset(HeapNode * heap_node_arr,
                             size_t idx,
                             size_t first, size_t last) -> size_t
            {
                if (first + 1 == last)
                {
                    return first;
                }

                if (heap_node_arr[idx].c == 0u)
                {
                    std::abort();
                }
                
                size_t interval_sz                      = last - first;
                size_t subsegment_sz                    = interval_sz / OUT_DEGREE_SIZE_VALUE;
                auto [rel_child_idx, is_right_trace]    = this->read_trace(heap_node_arr[idx].trace);
                size_t child_idx                        = (idx * OUT_DEGREE_SIZE_VALUE + rel_child_idx) + 1u;
                size_t next_first                       = first + subsegment_sz * rel_child_idx;
                size_t next_last                        = next_first + subsegment_sz;

                if (is_right_trace)
                {
                    return next_last - heap_node_arr[child_idx].r;
                }

                return this->seek_offset(heap_node_arr,
                                         child_idx,
                                         next_first, next_last);
            }

            auto seek_block() -> std::optional<std::pair<size_t, size_t>>
            {
                if (this->heap_node_arr_sz == 0u)
                {
                    std::abort();
                }

                size_t sz = this->heap_node_arr[0].c;

                if (sz == 0u)
                {
                    return std::nullopt;
                }

                size_t offset = this->seek_offset(heap_node_arr.get(),
                                                  0u,
                                                  0u, this->get_base_size());

                return std::make_pair(offset, sz);
            }

            auto intersect(const std::pair<size_t, size_t>& lhs,
                           const std::pair<size_t, size_t>& rhs) -> std::pair<size_t, size_t>
            {
                size_t lhs_first        = lhs.first;
                size_t lhs_last         = lhs.first + lhs.second;
                size_t rhs_first        = rhs.first;
                size_t rhs_last         = rhs.first + rhs.second;

                size_t first            = std::max(lhs_first, rhs_first);
                size_t tentative_last   = std::min(lhs_last, rhs_last);
                size_t last             = std::max(first, tentative_last);

                return {first, last - first};
            }

            auto unsigned_pow(size_t base, size_t sz) -> size_t
            {
                if (sz == 0u)
                {
                    return 1u;
                }

                size_t half_sz  = sz / 2;
                size_t half_pow = this->unsigned_pow(base, half_sz);
                size_t full_pow = half_pow * half_pow;

                if (sz % 2 != 0u)
                {
                    full_pow *= base;
                }

                return full_pow;
            }

            void make_heap_node_vec_helper(HeapNode * heap_node_arr,
                                           size_t idx,
                                           size_t first, size_t last)
            {
                heap_node_arr[idx] = HeapNode
                {
                    .l      = static_cast<NodeType>(last - first),
                    .r      = static_cast<NodeType>(last - first),
                    .c      = static_cast<NodeType>(last - first),
                    .trace  = static_cast<NodeType>(make_trace(0u, true))
                };

                if (first + 1 == last)
                {
                    return;
                }

                size_t interval_sz      = last - first;
                size_t subsegment_sz    = interval_sz / OUT_DEGREE_SIZE_VALUE;

                for (size_t i = 0u; i < OUT_DEGREE_SIZE_VALUE; ++i)
                {
                    size_t child_idx        = idx * OUT_DEGREE_SIZE_VALUE + (i + 1);
                    size_t segment_first    = first + subsegment_sz * i;
                    size_t segment_last     = segment_first + subsegment_sz;

                    this->make_heap_node_vec_helper(heap_node_arr,
                                                    child_idx,
                                                    segment_first, segment_last);
                }
            }

            auto make_heap_node_vec(size_t tree_height) -> std::pair<std::unique_ptr<HeapNode[]>, size_t>
            {
                if (tree_height == 0u)
                {
                    throw std::invalid_argument("bad tree height, 0");
                }

                size_t base_sz  = this->unsigned_pow(OUT_DEGREE_SIZE_VALUE, tree_height - 1u);
                size_t full_sz  = (base_sz * OUT_DEGREE_SIZE_VALUE - 1u) / (OUT_DEGREE_SIZE_VALUE - 1u);
                size_t idx      = 0u;

                std::unique_ptr<HeapNode[]> rs = std::make_unique<HeapNode[]>(full_sz);

                this->make_heap_node_vec_helper(rs.get(),
                                                idx,
                                                0u, base_sz);

                //eqn: x^0 + x^1 + x^2 + x^3 + ... = (x^(n + 1)) / (x - 1)
                //proof: f(x) * (x - 1) = x^(n + 1) - x^0
                //f(x) = (x^(n + 1) - 1) / (x - 1)

                return std::make_pair(std::move(rs), full_sz);
            }

            void rebind_heap_node(HeapNode * heap_node_arr,
                                  size_t idx,
                                  size_t interval_first, size_t interval_last)
            {
                if (interval_first + 1 >= interval_last)
                {
                    std::abort();
                }

                size_t left_sz          = 0u;
                size_t interval_sz      = interval_last - interval_first;
                size_t subinterval_sz   = interval_sz / OUT_DEGREE_SIZE_VALUE;

                for (size_t i = 0u; i < OUT_DEGREE_SIZE_VALUE; ++i)
                {
                    size_t child_idx    = idx * OUT_DEGREE_SIZE_VALUE + (i + 1);
                    size_t new_left_sz  = heap_node_arr[child_idx].l;
                    left_sz             += new_left_sz;

                    if (new_left_sz != subinterval_sz)
                    {
                        break;
                    }
                }

                size_t right_sz         = 0u;

                for (size_t i = 0u; i < OUT_DEGREE_SIZE_VALUE; ++i)
                {
                    size_t child_idx    = idx * OUT_DEGREE_SIZE_VALUE + (OUT_DEGREE_SIZE_VALUE - i);
                    size_t new_right_sz = heap_node_arr[child_idx].r;
                    right_sz            += new_right_sz;

                    if (new_right_sz != subinterval_sz)
                    {
                        break;
                    }
                }

                size_t other_c_best_sum     = 0u;
                size_t other_c_best_first   = 0u;

                for (size_t i = 0u; i < OUT_DEGREE_SIZE_VALUE; ++i)
                {
                    size_t child_idx        = idx * OUT_DEGREE_SIZE_VALUE + (i + 1);
                    size_t c1_value         = heap_node_arr[child_idx].c;

                    if (other_c_best_sum < c1_value)
                    {
                        other_c_best_sum      = c1_value;
                        other_c_best_first    = i;
                    }
                }

                size_t c_first              = 0u;
                size_t c_sum                = 0u;
                size_t c_best_sum           = 0u;
                size_t c_best_first         = 0u;

                for (size_t i = 0u; i < OUT_DEGREE_SIZE_VALUE; ++i)
                {
                    size_t child_idx        = idx * OUT_DEGREE_SIZE_VALUE + (i + 1);
                    size_t c_value          = heap_node_arr[child_idx].l;
                    c_sum                   += c_value;

                    if (c_value != subinterval_sz)
                    {
                        if (c_best_sum < c_sum)
                        {
                            c_best_sum      = c_sum;
                            c_best_first    = c_first;
                        }

                        size_t r_value  = heap_node_arr[child_idx].r;
                        c_first         = i + static_cast<size_t>(r_value == 0u);
                        c_sum           = r_value;
                    }
                }

                bool is_right_trace         = true;

                if (c_best_sum < c_sum)
                {
                    c_best_sum      = c_sum;
                    c_best_first    = c_first;
                }

                if (c_best_sum <= other_c_best_sum)
                {
                    c_best_sum      = other_c_best_sum;
                    c_best_first    = other_c_best_first;
                    is_right_trace  = false;
                }

                heap_node_arr[idx] = HeapNode
                {
                    .l      = static_cast<NodeType>(left_sz),
                    .r      = static_cast<NodeType>(right_sz),
                    .c      = static_cast<NodeType>(c_best_sum),
                    .trace  = static_cast<NodeType>(this->make_trace(c_best_first, is_right_trace))
                };
            }

            void block_interval_helper(HeapNode * heap_node_arr,
                                       size_t idx,
                                       size_t block_first, size_t block_last,
                                       size_t interval_first, size_t interval_last)
            {
                if (block_first == interval_first && block_last == interval_last)
                {
                    heap_node_arr[idx] = HeapNode
                    {
                        .l      = static_cast<NodeType>(0),
                        .r      = static_cast<NodeType>(0),
                        .c      = static_cast<NodeType>(0),
                        .trace  = BLOCKED_TRACE
                    };

                    return;
                }

                size_t interval_sz      = interval_last - interval_first;
                size_t subsegment_sz    = interval_sz / OUT_DEGREE_SIZE_VALUE;

                //let's do extreme optimizations here

                for (size_t i = 0u; i < OUT_DEGREE_SIZE_VALUE; ++i)
                {
                    size_t child_idx                            = idx * OUT_DEGREE_SIZE_VALUE + (i + 1);
                    size_t nxt_interval_first                   = interval_first + subsegment_sz * i;
                    size_t nxt_interval_last                    = nxt_interval_first + subsegment_sz;
                    auto [intersected_first, intersected_sz]    = this->intersect({block_first, block_last - block_first}, {nxt_interval_first, subsegment_sz});

                    if (intersected_sz != 0u)
                    {
                        this->block_interval_helper(heap_node_arr,
                                                    child_idx,
                                                    intersected_first, intersected_first + intersected_sz,
                                                    nxt_interval_first, nxt_interval_last);
                    }
                }

                this->rebind_heap_node(heap_node_arr, idx, interval_first, interval_last);
            }

            void block_interval(const std::pair<size_t, size_t>& interval)
            {
                size_t first        = interval.first;
                size_t last         = interval.first + interval.second;
                size_t idx          = 0u;
                size_t tree_first   = 0u;
                size_t tree_last    = this->get_base_size();

                if (first >= last)
                {
                    throw std::invalid_argument("bad interval, <= 0");
                }

                if (last > tree_last)
                {
                    throw std::invalid_argument("bad interval, out of bound");
                }

                this->block_interval_helper(this->heap_node_arr.get(),
                                            idx,
                                            first, last,
                                            tree_first, tree_last);
            }

            void unblock_interval_helper(HeapNode * heap_node_arr,
                                         size_t idx,
                                         size_t unblock_first, size_t unblock_last,
                                         size_t interval_first, size_t interval_last)
            {
                if (unblock_first == interval_first && unblock_last == interval_last)
                {
                    heap_node_arr[idx] = HeapNode
                    {
                        .l      = static_cast<NodeType>(interval_last - interval_first),
                        .r      = static_cast<NodeType>(interval_last - interval_first),
                        .c      = static_cast<NodeType>(interval_last - interval_first),
                        .trace  = static_cast<NodeType>(make_trace(0u, true))
                    };

                    return;
                }

                size_t interval_sz      = interval_last - interval_first;
                size_t subsegment_sz    = interval_sz / OUT_DEGREE_SIZE_VALUE;

                for (size_t i = 0u; i < OUT_DEGREE_SIZE_VALUE; ++i)
                {
                    size_t child_idx                            = idx * OUT_DEGREE_SIZE_VALUE + (i + 1);
                    size_t nxt_interval_first                   = interval_first + subsegment_sz * i;
                    size_t nxt_interval_last                    = nxt_interval_first + subsegment_sz;
                    auto [intersected_first, intersected_sz]    = this->intersect({unblock_first, unblock_last - unblock_first}, {nxt_interval_first, subsegment_sz});

                    if (intersected_sz != 0u)
                    {
                        this->unblock_interval_helper(heap_node_arr,
                                                      child_idx,
                                                      intersected_first, intersected_first + intersected_sz,
                                                      nxt_interval_first, nxt_interval_last);
                    }
                }

                this->rebind_heap_node(heap_node_arr, idx, interval_first, interval_last);
            }

            void unblock_interval(const std::pair<size_t, size_t>& interval)
            {
                size_t first        = interval.first;
                size_t last         = interval.first + interval.second;
                size_t idx          = 0u;
                size_t tree_first   = 0u;
                size_t tree_last    = this->get_base_size();

                if (first >= last)
                {
                    throw std::invalid_argument("bad interval, <= 0");
                }

                if (last > tree_last)
                {
                    throw std::invalid_argument("bad interval, out of bound");
                }

                this->unblock_interval_helper(this->heap_node_arr.get(),
                                              idx,
                                              first, last,
                                              tree_first, tree_last);
            }
    };
}

#endif