#ifndef __GLOBAL_ALLOCATOR_H__
#define __GLOBAL_ALLOCATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <algorithm>
#include <utility>
#include <functional>
#include <optional>
#include <cstdlib>
#include "assert.h"
#include "stdx.h"
#include "unordered_node_map.h"

//our best bet to do allocator is actually two management trees with best-block allocation
//this is concluded after 3 years of coding allocators, because I can't find a reasonable way to de-fragment the memory
//we'd try to increase the outdegree this time
//since this is actually important, though can be solved by a kernel reset, I dont know if this is worth it
//we dont have anything to do, so ..., let's just write this, and we'd actually assign 64KB segment pages to have fast memory accesses
//I also think that we'd have to do everything we could to avoid a hard reset due to fragmentation

//I've spent literally too much time writing allocators, from page reference to scope allocator to stack allocator to interval tree heap allocator to etc.
//the best allocator that is actually C++ compatible is the interval tree allocator

//this is due to the fact of equilibrium of persistent memory and lifetime memory
//lifetime memory is the memory that takes a certain time to be freed
//and the lifetime memory must take less than the time it takes for the other interval tree to be completely fragmented
//we are also cache-aware people, because this allocator takes too much cache to be useable, it pollutes the cache page very badly, so we'd have to only use dynamic memory allocations in the worst possible time, this is not negotiable

namespace global_allocator
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

            constexpr auto make_trace(size_t offset, bool is_right_trace) const noexcept -> size_t
            {
                return (offset << 1) | static_cast<size_t>(is_right_trace);
            }

            constexpr auto read_trace(size_t trace) const noexcept -> std::pair<size_t, bool>
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

    class BinaryUnitAllocator
    {
        private:

            unordered_map_variants::unordered_node_map<uint8_t, size_t, uint8_t> unit_counter;
            unordered_map_variants::unordered_node_map<uint8_t, std::vector<char *>, uint8_t> unit_map;
            std::vector<std::unique_ptr<char[]>> raii_buf_vec;

            static inline constexpr size_t MIN_POW2_VALUE   = 5u;            
            using header_t = uint8_t;

        public:

            auto malloc(size_t sz) -> char *
            {
                if (sz == 0u) [[unlikely]]
                {
                    return nullptr;
                }

                size_t full_sz      = sz + sizeof(header_t);
                size_t bucket_idx   = std::max(static_cast<size_t>(stdx::ulog2(stdx::ceil2(full_sz))), MIN_POW2_VALUE);
                auto map_ptr        = this->unit_map.find(bucket_idx);

                if (map_ptr == this->unit_map.end() || map_ptr->second.empty()) [[unlikely]]
                {
                    return this->enqueue_new_buffer_of_bucket_idx(bucket_idx);
                }
                else [[likely]]
                {
                    char * result = map_ptr->second.back();
                    map_ptr->second.pop_back();

                    return result;
                }
            }

            __attribute__((noipa, noinline)) void free(void * buf) noexcept
            {
                if (buf == nullptr) [[unlikely]]
                {
                    return;
                }

                auto [org_buf, bucket_idx]  = this->to_original_buffer(static_cast<char *>(buf));
                auto map_ptr                = this->unit_map.find(bucket_idx);

                if (map_ptr == this->unit_map.end())
                {
                    std::abort();
                }

                map_ptr->second.push_back(static_cast<char *>(buf));
            }

        private:

            __attribute__((noinline)) auto enqueue_new_buffer_of_bucket_idx(size_t bucket_idx) -> char *
            {
                size_t actual_sz                = size_t{1} << bucket_idx;
                std::unique_ptr<char[]> buf     = std::make_unique<char[]>(actual_sz);
                char * buf_value                = this->internal_write_metadata_to_buffer(buf.get(), bucket_idx);

                this->raii_buf_vec.push_back(std::move(buf));

                auto [insert_ptr, status]       = [&]
                {
                    try
                    {
                        return this->unit_map.insert({bucket_idx, std::vector<char *>{}});
                    }
                    catch (...)
                    {
                        this->raii_buf_vec.pop_back();
                        throw;
                    }
                }();

                auto [unit_ptr, unit_status]    = [&]
                {
                    try
                    {
                        return this->unit_counter.insert({bucket_idx, 0u});
                    }
                    catch (...)
                    {
                        if (status)
                        {
                            this->unit_map.erase(insert_ptr);
                        }

                        this->raii_buf_vec.pop_back();
                        throw;
                    }
                }();

                size_t new_sz           = unit_ptr->second + 1u;
                size_t reservation_sz   = stdx::ceil2(new_sz);

                try
                {
                    insert_ptr->second.reserve(reservation_sz);
                }
                catch (...)
                {
                    if (unit_status)
                    {
                        this->unit_counter.erase(unit_ptr);
                    }

                    if (status)
                    {
                        this->unit_map.erase(insert_ptr);
                    }

                    this->raii_buf_vec.pop_back();
                    throw;
                }

                unit_ptr->second = new_sz;

                return buf_value;
            }

            constexpr auto internal_write_metadata_to_buffer(char * buf, header_t bucket_idx) const noexcept -> char *
            {
                std::memcpy(buf, &bucket_idx, sizeof(header_t));

                return std::next(buf, sizeof(header_t));
            }

            constexpr auto to_original_buffer(char * buf) const noexcept -> std::pair<char *, header_t>
            {
                char * previous_buf = std::prev(buf, sizeof(header_t));
                header_t bucket_idx;
                std::memcpy(&bucket_idx, previous_buf, sizeof(header_t));

                return {previous_buf, bucket_idx};
            }
    };

    class ThreadSafeBinaryUnitAllocator: private BinaryUnitAllocator
    {
        private:

            fair_mutex::fair_atomic_flag mtx;
        
        public:

            ThreadSafeBinaryUnitAllocator(): BinaryUnitAllocator()
            {
                fair_mutex::inplace_make_fair_atomic_flag(this->mtx);
            }

            auto malloc(size_t sz) -> char *
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(this->mtx);

                return BinaryUnitAllocator::malloc(sz);
            }

            void free(void * buf) noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(this->mtx);

                BinaryUnitAllocator::free(buf);
            }
    };

    class MultithreadedBinaryUnitAllocator
    {
        private:

            std::vector<std::unique_ptr<ThreadSafeBinaryUnitAllocator>> allocation_vec;

        public:

            MultithreadedBinaryUnitAllocator(size_t concurrent_sz): allocation_vec()
            {
                size_t ceil_sz  = stdx::ceil2(concurrent_sz);
                size_t max_idx  = ceil_sz - 1u;

                if (max_idx > std::numeric_limits<uint8_t>::max())
                {
                    throw std::invalid_argument("max concurrent allocation reached, std::numeric_limits<uint8_t>::max()");
                }

                this->allocation_vec.reserve(ceil_sz);

                for (size_t i = 0u; i < ceil_sz; ++i)
                {
                    this->allocation_vec.push_back(std::make_unique<ThreadSafeBinaryUnitAllocator>());
                }
            }

            MultithreadedBinaryUnitAllocator(): MultithreadedBinaryUnitAllocator(1){}

            auto malloc(size_t sz) -> char *
            {
                size_t thr_id       = std::bit_cast<size_t>(std::this_thread::get_id());
                uint8_t slot_id     = thr_id & (this->allocation_vec.size() - 1u);
                size_t actual_sz    = sz + sizeof(uint8_t);
                char * internal_rs  = this->allocation_vec[slot_id]->malloc(actual_sz);

                std::memcpy(internal_rs, &slot_id, sizeof(uint8_t));

                return std::next(internal_rs, sizeof(uint8_t));
            }

            __attribute__((noipa, noinline)) void free(void * buf) noexcept
            {
                size_t thr_id       = std::bit_cast<size_t>(std::this_thread::get_id());
                char * internal_rs  = std::prev(static_cast<char *>(buf), 1u);
                uint8_t slot_id;
                std::memcpy(&slot_id, internal_rs, sizeof(uint8_t));

                this->allocation_vec[slot_id]->free(static_cast<void *>(internal_rs));
            }
    };

    static inline constexpr size_t THREAD_CONCURRENT_SZ = 1u;
    static_assert(THREAD_CONCURRENT_SZ > 0u);

    using allocator_t = std::conditional_t<THREAD_CONCURRENT_SZ == 1u,
                                           BinaryUnitAllocator,
                                           MultithreadedBinaryUnitAllocator>;

    struct Signature{};
    using singleton_instance = stdx::singleton_container<allocator_t, Signature>;

    void init()
    {
        if constexpr(THREAD_CONCURRENT_SZ == 1u)
        {
            singleton_instance::get() = BinaryUnitAllocator();
        }
        else
        {
            // singleton_instance::get() = MultithreadedBinaryUnitAllocator(THREAD_CONCURRENT_SZ);
        }
    }

    // static volatile int lazy_initializer = []
    // {
    //     init();
    //     return 1;
    // }();

    void deinit() noexcept
    {
        singleton_instance::get() = {};
    }

    template <class T, std::enable_if_t<std::numeric_limits<T>::is_integer, bool> = true>
    static constexpr auto is_pow2(T value) noexcept -> bool
    {
        if (value == 0u)
        {
            return false;
        }

        T value_one = value - 1u; //godzilla
        return (value & value_one) == 0u;
    }

    template <uintptr_t ALIGNMENT_SZ>
    static constexpr auto align(uintptr_t ptr, const std::integral_constant<uintptr_t, ALIGNMENT_SZ>) noexcept -> uintptr_t
    {
        static_assert(is_pow2(ALIGNMENT_SZ));

        uintptr_t fwd_sz    = ALIGNMENT_SZ - 1u;
        uintptr_t bit_mask  = ~fwd_sz;
        uintptr_t fwd_ptr   = ptr + fwd_sz;

        return fwd_ptr & bit_mask;
    }

    static constexpr auto align(uintptr_t ptr, uintptr_t alignment_sz) noexcept -> uintptr_t
    {
        assert(is_pow2(alignment_sz));

        uintptr_t fwd_sz    = alignment_sz - 1u;
        uintptr_t bit_mask  = ~fwd_sz;
        uintptr_t fwd_ptr   = ptr + fwd_sz;

        return fwd_ptr & bit_mask;
    }   

    template <class T>
    __attribute__((noinline)) auto object_malloc(size_t object_count) -> char *
    {
        if (object_count == 0u)
        {
            return nullptr;
        }

        static_assert(sizeof(T) != 0u);
        static_assert(alignof(T) != 0u);

        constexpr size_t FWD_SZ     = alignof(T) - 1u;
        static_assert(FWD_SZ <= std::numeric_limits<uint16_t>::max());

        size_t sz                   = object_count * sizeof(T);
        size_t total_sz             = sz + sizeof(uint16_t);
        size_t aligned_total_sz     = total_sz + FWD_SZ;

        char * buf                  = singleton_instance::get().malloc(aligned_total_sz);
        char * fwd_buf              = std::next(buf, sizeof(uint16_t));
        char * aligned_fwd_buf      = reinterpret_cast<char *>(align(reinterpret_cast<uintptr_t>(fwd_buf), std::integral_constant<uintptr_t, alignof(T)>{}));

        char * prev_aligned_fwd_buf = std::prev(aligned_fwd_buf, sizeof(uint16_t));
        uint16_t dist               = std::distance(fwd_buf, aligned_fwd_buf);

        std::memcpy(prev_aligned_fwd_buf, &dist, sizeof(uint16_t));

        return aligned_fwd_buf;
    }

    __attribute__((noinline)) void object_free(void * buf) noexcept
    {
        if (buf == nullptr)
        {
            return;
        }

        char * char_buf = static_cast<char *>(buf);
        char * prev_buf = std::prev(char_buf, sizeof(uint16_t));

        uint16_t dist;
        std::memcpy(&dist, prev_buf, sizeof(uint16_t));

        char * fwd_buf  = std::prev(char_buf, dist);
        char * org_buf  = std::prev(fwd_buf, sizeof(uint16_t));

        singleton_instance::get().free(org_buf);
    }

    template <class T>
    class GlobalAllocator
    {
        private:

            template <class U>
            friend class GlobalAllocator;

        public:

            using value_type = T;
            using pointer = T*;
            using const_pointer = const T*;
            using reference = T&;
            using const_reference = const T&;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;

            template <class U>
            struct rebind {
                using other = GlobalAllocator<U>;
            };

            constexpr GlobalAllocator(){}

            template <class U>
            constexpr GlobalAllocator(const GlobalAllocator<U>& other){}

            constexpr auto allocate(size_t n) -> T *
            {
                return static_cast<T *>(static_cast<void *>(object_malloc<T>(n)));
            }

            constexpr void deallocate(T * ptr, size_t sz)
            {
                object_free(static_cast<void *>(ptr));
            }

            template <class ...Args>
            constexpr void construct(T * ptr, Args&& ...args)
            {
                new (ptr) T(std::forward<Args>(args)...);
            }

            constexpr void destroy(T * ptr)
            {
                std::destroy_at(ptr);
            }
    };
}

#endif