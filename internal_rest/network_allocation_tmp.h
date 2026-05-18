#ifndef __DG_NETWORK_ALLOCATION_H__
#define __DG_NETWORK_ALLOCATION_H__

//define HEADER_CONTROL 4

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <atomic>
#include <thread>
#include "assert.h"
#include <bit>
#include <vector>
#include "stdx.h"
#include "network_exception.h"
#include "network_concurrency.h"
#include <memory>
#include "network_trivial_serializer.h"
#include "network_datastructure.h"
#include "network_hash.h"
#include <bit>

namespace dg_sock::network_allocation
{
    template <class Key, class Value>
    using unordered_map         = dg_sock::network_datastructure::unordered_map_variants::unordered_node_map<Key, Value>;

    template <class Value>
    using pow2_cyclic_queue     = dg_sock::network_datastructure::cyclic_queue::pow2_cyclic_queue<Value>;

    class BinaryUnitAllocator
    {
        private:

            unordered_map<uint8_t, size_t> unit_counter;
            unordered_map<uint8_t, std::vector<char *>> unit_map;
            std::vector<std::shared_ptr<char[]>> raii_buf_vec;

            using header_t = uint8_t;

        public:

            static inline constexpr size_t DEFAULT_ALIGNMENT_SZ = 1u;

        private:

            static_assert(stdxx::is_pow2(DEFAULT_ALIGNMENT_SZ));
            static inline constexpr size_t MIN_POW2_VALUE       = std::max(size_t{5u}, static_cast<size_t>(stdxx::ulog2(DEFAULT_ALIGNMENT_SZ)));

        public:

            inline auto malloc(size_t sz) -> void *
            {
                if (sz == 0u) [[unlikely]]
                {
                    return nullptr;
                }

                size_t full_sz      = sz + sizeof(header_t);
                size_t bucket_idx   = std::max(static_cast<size_t>(stdxx::ulog2(stdxx::ceil2(full_sz))), MIN_POW2_VALUE);
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

            inline void free(void * buf) noexcept
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

            constexpr auto bucket_idx_to_buffer_size(size_t bucket_idx) const noexcept -> size_t
            {
                return size_t{1} << bucket_idx;
            }

            __attribute__((noinline)) auto enqueue_new_buffer_of_bucket_idx(size_t bucket_idx) -> char *
            {
                size_t actual_sz                = this->bucket_idx_to_buffer_size(bucket_idx);

                auto std_memory_free_func       = [](char * mem) noexcept
                {
                    std::free(mem);
                };

                std::shared_ptr<char[]> buf     = std::unique_ptr<char[], decltype(std_memory_free_func)>(static_cast<char *>(std::malloc(actual_sz)), std_memory_free_func);

                if (buf == nullptr)
                {
                    throw std::bad_alloc();
                }

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
                size_t reservation_sz   = stdxx::ceil2(new_sz);

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

            stdxx::fair_atomic_flag mtx;

        public:

            static inline constexpr size_t DEFAULT_ALIGNMENT_SZ = BinaryUnitAllocator::DEFAULT_ALIGNMENT_SZ;

            ThreadSafeBinaryUnitAllocator(): BinaryUnitAllocator()
            {
                stdxx::inplace_make_fair_atomic_flag(this->mtx);
            }

            inline auto malloc(size_t sz) -> void *
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(this->mtx);

                return BinaryUnitAllocator::malloc(sz);
            }

            inline void free(void * buf) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(this->mtx);

                BinaryUnitAllocator::free(buf);
            }
    };

    template <size_t CONCURRENCY_SZ_ARG>
    class DistributedThreadSafeBinaryUnitAllocator
    {
        private:

            std::vector<std::unique_ptr<ThreadSafeBinaryUnitAllocator>> allocator_vec;

        public:

            static inline constexpr size_t CONCURRENCY_SZ = CONCURRENCY_SZ_ARG;

            static_assert(CONCURRENCY_SZ <= std::numeric_limits<uint8_t>::max());
            static_assert(stdxx::is_pow2(CONCURRENCY_SZ));

            static inline constexpr size_t DEFAULT_ALIGNMENT_SZ = 1u;

            DistributedThreadSafeBinaryUnitAllocator(): allocator_vec()
            {
                for (size_t i = 0u; i < CONCURRENCY_SZ; ++i)
                {
                    this->allocator_vec.push_back(std::make_unique<ThreadSafeBinaryUnitAllocator>());
                }
            }

            inline auto malloc(size_t sz) -> void *
            {
                if (sz == 0u) [[unlikely]]
                {
                    return nullptr;
                }

                uint8_t idx     = dg_sock::network_hash::hash_reflectible(std::bit_cast<size_t>(std::this_thread::get_id())) & (CONCURRENCY_SZ - 1u);
                size_t new_sz   = sz + sizeof(uint8_t);
                void * rs       = this->allocator_vec[idx]->malloc(new_sz);

                std::memcpy(rs, &idx, sizeof(uint8_t));

                return std::next(static_cast<char *>(rs), sizeof(uint8_t));
            }

            inline void free(void * buf) noexcept
            {
                if (buf == nullptr) [[unlikely]]
                {
                    return;
                }

                uint8_t idx;
                char * buf_head = std::prev(static_cast<char *>(buf), sizeof(uint8_t));
                std::memcpy(&idx, buf_head, sizeof(uint8_t));

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->allocator_vec.size())
                    {
                        std::abort();
                    }
                }

                this->allocator_vec[idx]->free(buf_head);
            }
    };

    static inline constexpr size_t BINARY_UNIT_ALLOCATOR_CONCURRENCY_SZ = 4u;

    using BestBinaryUnitAllocator = std::conditional_t<(BINARY_UNIT_ALLOCATOR_CONCURRENCY_SZ == 1u),
                                                       ThreadSafeBinaryUnitAllocator,
                                                       DistributedThreadSafeBinaryUnitAllocator<BINARY_UNIT_ALLOCATOR_CONCURRENCY_SZ>>;

    class AffinedAllocator
    {
        private:

            struct MemoryPool
            {
                pow2_cyclic_queue<void *> pool;
            };

            std::vector<MemoryPool> pool_vec;
            std::shared_ptr<BestBinaryUnitAllocator> base_allocator;
            unordered_map<size_t, pow2_cyclic_queue<void *>> cache_map;

        public:

            static inline constexpr size_t FLUSH_THRESHOLD                  = size_t{1} << 24;
            static inline constexpr size_t STACK_CAPTURE_POOL_SZ            = size_t{1} << 6;
            static inline constexpr size_t STACK_CAPTURE_POOL_POPULATION    = size_t{1} << 6;
            static inline constexpr size_t DEFAULT_ALIGNMENT_SZ             = std::min(BestBinaryUnitAllocator::DEFAULT_ALIGNMENT_SZ, static_cast<size_t>(sizeof(uint32_t)));

            AffinedAllocator(std::shared_ptr<BestBinaryUnitAllocator> base_allocator)
            {
                if (base_allocator == nullptr)
                {
                    throw std::invalid_argument("bad base allocator, null");
                }

                this->pool_vec          = {};
                this->base_allocator    = std::move(base_allocator);
                this->cache_map         = {};

                for (size_t i = 0u; i < STACK_CAPTURE_POOL_SZ; ++i)
                {
                    pow2_cyclic_queue<void *> pool(stdxx::ulog2(stdxx::ceil2(STACK_CAPTURE_POOL_POPULATION)));

                    this->pool_vec.push_back(MemoryPool
                    {
                        .pool = std::move(pool)
                    });
                }
            }

            ~AffinedAllocator() noexcept
            {
                this->flush();
            }

            inline auto malloc(size_t sz) -> void *
            {
                auto map_ptr = this->cache_map.find(sz);

                if (map_ptr != this->cache_map.end() && map_ptr->second.size() > 1) [[likely]]
                {
                    void * rs = map_ptr->second.back();
                    map_ptr->second.pop_back();

                    return rs;
                }
                else [[unlikely]]
                {
                    return this->slow_malloc(sz);
                }
            }

            inline void free(void * buf) noexcept
            {
                if (buf == nullptr) [[unlikely]]
                {
                    return;
                }

                void * previous_buf     = std::prev(static_cast<char *>(buf), sizeof(uint32_t));

                uint32_t buf_usr_sz;
                std::memcpy(&buf_usr_sz, previous_buf, sizeof(uint32_t));

                auto map_ptr            = this->cache_map.find(buf_usr_sz);

                if (map_ptr != this->cache_map.end() && map_ptr->second.size() != map_ptr->second.capacity()) [[likely]]
                {
                    exception_t err = map_ptr->second.push_back(buf);

                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (dg_sock::network_exception::is_failed(err))
                        {
                            std::abort();
                        }
                    }
                }
                else [[unlikely]]
                {
                    this->slow_free(buf);
                }
            }

        private:

            __attribute__((noinline)) auto slow_malloc(size_t sz) -> void *
            {
                if (sz == 0u)
                {
                    return nullptr;
                }

                auto map_ptr = this->cache_map.find(sz);

                if (map_ptr != this->cache_map.end())
                {
                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (map_ptr->second.empty())
                        {
                            std::abort();
                        }
                    }

                    void * rs = map_ptr->second.back();
                    map_ptr->second.pop_back();

                    if (map_ptr->second.empty())
                    {
                        this->pool_vec.push_back(MemoryPool
                        {
                            .pool = std::move(map_ptr->second)
                        });

                        this->cache_map.erase(map_ptr);
                    }

                    return rs;
                }

                if (sz > std::numeric_limits<uint32_t>::max())
                {
                    std::abort();
                }

                size_t actual_sz    = sz + sizeof(uint32_t);
                void * rs           = this->base_allocator->malloc(actual_sz);
                uint32_t u32_sz     = sz;

                std::memcpy(rs, &u32_sz, sizeof(uint32_t));

                return std::next(static_cast<char *>(rs), sizeof(uint32_t));
            }

            __attribute__((noinline)) void slow_free(void * buf) noexcept
            {
                if (buf == nullptr) [[unlikely]]
                {
                    return;
                }

                void * previous_buf     = std::prev(static_cast<char *>(buf), sizeof(uint32_t));

                uint32_t buf_usr_sz;
                std::memcpy(&buf_usr_sz, previous_buf, sizeof(uint32_t));

                auto map_ptr            = this->cache_map.find(buf_usr_sz);

                if (map_ptr == this->cache_map.end())
                {
                    if (this->pool_vec.empty())
                    {
                        this->flush();
                    }

                    try
                    {
                        auto [new_map_ptr, status] = this->cache_map.insert(std::make_pair(buf_usr_sz, std::move(this->pool_vec.back().pool)));
                        this->pool_vec.pop_back();
                        assert(status);
                        map_ptr = new_map_ptr;
                    }
                    catch (...)
                    {
                        std::abort();
                    }
                }

                if (map_ptr->second.size() == map_ptr->second.capacity())
                {
                    this->free_user_ptr(map_ptr->second.front());
                    map_ptr->second.pop_front();
                }

                exception_t err = map_ptr->second.push_back(buf);

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (dg_sock::network_exception::is_failed(err))
                    {
                        std::abort();
                    }
                }
            }

            inline void free_user_ptr(void * ptr) noexcept
            {
                this->base_allocator->free(std::prev(static_cast<char *>(ptr), sizeof(uint32_t)));
            }

            void flush() noexcept
            {
                for (auto& map_pair: this->cache_map)
                {
                    for (void * mempiece: map_pair.second)
                    {
                        this->free_user_ptr(mempiece);
                    }

                    map_pair.second.clear();
                    this->pool_vec.push_back(MemoryPool
                    {
                        .pool = std::move(map_pair.second)
                    });
                }

                this->cache_map.clear();
            }
    };

    class LargeSmallAffinedAllocator: private AffinedAllocator
    {
        private:

            std::shared_ptr<BestBinaryUnitAllocator> _allocator;

            static inline constexpr uint8_t SMALL_ALLOCATION_FLAG       = 0u;
            static inline constexpr uint8_t LARGE_ALLOCATION_FLAG       = 1u;

        public:

            static inline constexpr size_t DEFAULT_ALIGNMENT_SZ         = 1u;
            static inline constexpr size_t LARGE_BUFFER_SIZE            = size_t{1} << 18;

            LargeSmallAffinedAllocator(std::shared_ptr<BestBinaryUnitAllocator> allocator): AffinedAllocator(allocator)
            {
                if (allocator == nullptr)
                {
                    throw std::invalid_argument("bad allocator, null");
                }

                this->_allocator = allocator;
            }

            inline auto malloc(size_t sz) -> void *
            {
                if (sz == 0u) [[unlikely]]
                {
                    return nullptr;
                }

                if (sz < LARGE_BUFFER_SIZE) [[likely]]
                {
                    return this->small_malloc(sz);
                }
                else [[unlikely]]
                {
                    return this->large_malloc(sz);
                }
            }

            inline void free(void * buf) noexcept
            {
                if (buf == nullptr)
                {
                    return;
                }

                uint8_t header = this->read_allocation_header(buf);

                if (header == SMALL_ALLOCATION_FLAG) [[likely]]
                {
                    this->small_free(buf);
                }
                else [[unlikely]]
                {
                    this->large_free(buf);
                }
            }

        private:

            inline auto to_internal_ptr(void * usr_ptr) noexcept -> void *
            {
                return std::prev(static_cast<char *>(usr_ptr), sizeof(uint8_t));
            }

            inline auto read_allocation_header(void * usr_ptr) noexcept -> uint8_t
            {
                void * prev_usr_ptr = this->to_internal_ptr(usr_ptr);
                uint8_t rs;

                std::memcpy(&rs, prev_usr_ptr, sizeof(uint8_t));

                return rs;
            }

            inline auto small_malloc(size_t sz) -> void *
            {
                size_t new_sz   = sz + sizeof(uint8_t);
                void * rs       = AffinedAllocator::malloc(new_sz);

                std::memcpy(rs, &SMALL_ALLOCATION_FLAG, sizeof(uint8_t));

                return std::next(static_cast<char *>(rs), sizeof(uint8_t));
            }

            inline void small_free(void * usr_ptr) noexcept
            {
                AffinedAllocator::free(this->to_internal_ptr(usr_ptr));
            }

            __attribute__((noinline)) auto large_malloc(size_t sz) -> void *
            {
                size_t new_sz   = sz + sizeof(uint8_t);
                void * rs       = this->_allocator->malloc(new_sz);

                std::memcpy(rs, &LARGE_ALLOCATION_FLAG, sizeof(uint8_t));

                return std::next(static_cast<char *>(rs), sizeof(uint8_t));
            }

            __attribute__((noinline)) void large_free(void * usr_ptr) noexcept
            {
                this->_allocator->free(this->to_internal_ptr(usr_ptr));
            }

    };

    class RoundBucketAffinedAllocator: private LargeSmallAffinedAllocator
    {
        private:

            static inline constexpr size_t SLACK_BUFFER_SZ      = 8u;

        public:

            static inline constexpr size_t DEFAULT_ALIGNMENT_SZ = LargeSmallAffinedAllocator::DEFAULT_ALIGNMENT_SZ;

            RoundBucketAffinedAllocator(std::shared_ptr<BestBinaryUnitAllocator> base_allocator): LargeSmallAffinedAllocator(std::move(base_allocator)){}

            inline auto malloc(size_t sz) -> void *
            {
                return LargeSmallAffinedAllocator::malloc(this->promote_size(sz));
            }

            inline void free(void * buf) noexcept
            {
                LargeSmallAffinedAllocator::free(buf);
            }

        private:

            constexpr auto promote_size(size_t sz) const noexcept -> size_t
            {
                return stdxx::ceil2(sz + SLACK_BUFFER_SZ) - SLACK_BUFFER_SZ;
            }
    };

    class GlobalAllocator
    {
        private:

            std::vector<std::unique_ptr<RoundBucketAffinedAllocator>> affined_allocator_vec;

        public:

            static inline constexpr size_t DEFAULT_ALIGNMENT_SZ = RoundBucketAffinedAllocator::DEFAULT_ALIGNMENT_SZ;

            GlobalAllocator(): affined_allocator_vec()
            {
                std::shared_ptr<BestBinaryUnitAllocator> allocator = std::make_shared<BestBinaryUnitAllocator>();

                for (size_t i = 0u; i < dg_sock::network_concurrency::get_thread_count(); ++i)
                {
                    this->affined_allocator_vec.push_back(std::make_unique<RoundBucketAffinedAllocator>(allocator));
                }   
            }

            inline auto malloc(size_t sz) -> void *
            {
                size_t thr_idx  = dg_sock::network_concurrency::this_thread_idx();

                if (thr_idx >= this->affined_allocator_vec.size())
                {
                    std::abort();
                }

                return this->affined_allocator_vec[thr_idx]->malloc(sz);
            }

            inline void free(void * buf) noexcept
            {
                size_t thr_idx  = dg_sock::network_concurrency::this_thread_idx();

                if (thr_idx >= this->affined_allocator_vec.size())
                {
                    std::abort();
                }

                return this->affined_allocator_vec[thr_idx]->free(buf);
            }
    };

    struct AllocationResourceSignature{};

    using allocation_resource_obj = stdxx::singleton<AllocationResourceSignature, std::shared_ptr<GlobalAllocator>>; 

    void init()
    {
        allocation_resource_obj::get() = std::make_shared<GlobalAllocator>();
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void deinit() noexcept
    {
        allocation_resource_obj::get() = nullptr;
    }

    static inline constexpr size_t DEFAULT_ALIGNMENT_SZ = GlobalAllocator::DEFAULT_ALIGNMENT_SZ; 

    using alignment_header_t        = uint32_t;
    using xalign_metadata_size_t    = uint32_t;

    static constexpr auto dg_align(void * buf, uintptr_t alignment_sz) noexcept -> void *
    {
        assert(stdxx::is_pow2(alignment_sz));

        uintptr_t arithmetic_buf        = reinterpret_cast<uintptr_t>(buf);
        uintptr_t FWD_SZ                = alignment_sz - 1u;
        uintptr_t MASK_VALUE            = ~FWD_SZ;
        uintptr_t fwd_arithmetic_buf    = (arithmetic_buf + FWD_SZ) & MASK_VALUE;

        return reinterpret_cast<void *>(fwd_arithmetic_buf);
    }

    static constexpr auto dg_align(const void * buf, uintptr_t alignment_sz) noexcept -> const void *
    {
        assert(stdxx::is_pow2(alignment_sz));

        uintptr_t arithmetic_buf        = reinterpret_cast<uintptr_t>(buf);
        uintptr_t FWD_SZ                = alignment_sz - 1u;
        uintptr_t MASK_VALUE            = ~FWD_SZ;
        uintptr_t fwd_arithmetic_buf    = (arithmetic_buf + FWD_SZ) & MASK_VALUE;

        return reinterpret_cast<const void *>(fwd_arithmetic_buf);
    }

    extern auto dg_malloc(size_t blk_sz) -> void *
    {
        return allocation_resource_obj::get()->malloc(blk_sz); 
    }

    extern void dg_free(void * ptr) noexcept
    {
        allocation_resource_obj::get()->free(ptr);
    }

    extern auto dg_aligned_alloc(size_t alignment, size_t blk_sz) -> void *
    {
        if (!stdxx::is_pow2(alignment))
        {
            throw std::bad_alloc();
        }

        const size_t max_fwd_sz = alignment + (sizeof(alignment_header_t) - 1u);

        if (max_fwd_sz > std::numeric_limits<alignment_header_t>::max())
        {
            throw std::bad_alloc();
        }

        if (blk_sz == 0u)
        {
            return nullptr;
        }

        size_t adj_blk_sz   = blk_sz + max_fwd_sz;
        void * ptr          = allocation_resource_obj::get()->malloc(adj_blk_sz);

        if (ptr == nullptr)
        {
            std::abort();
        }

        void * aligned_ptr              = dg_sock::network_allocation::dg_align(std::next(static_cast<char *>(ptr), sizeof(alignment_header_t)), alignment);
        alignment_header_t difference   = std::distance(static_cast<char *>(ptr), static_cast<char *>(aligned_ptr));
        void * alignment_header_addr    = std::prev(static_cast<char *>(aligned_ptr), sizeof(alignment_header_t));

        std::memcpy(alignment_header_addr, &difference, sizeof(alignment_header_t));

        return aligned_ptr; 
    } 

    extern void dg_aligned_free(void * ptr) noexcept
    {
        if (ptr == nullptr)
        {
            return;
        }

        void * alignment_header_addr    = std::prev(static_cast<char *>(ptr), sizeof(alignment_header_t));
        alignment_header_t difference;
        std::memcpy(&difference, alignment_header_addr, sizeof(alignment_header_t));
        void * org_ptr                  = std::prev(static_cast<char *>(ptr), difference); 

        allocation_resource_obj::get()->free(org_ptr); 
    }

    struct XAlignMetadata
    {
        alignment_header_t difference;
        xalign_metadata_size_t blk_sz; 

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(difference, blk_sz);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(difference, blk_sz);
        }
    };

    extern auto dg_xaligned_alloc(size_t alignment, size_t blk_sz) -> void *
    {
        constexpr size_t METADATA_SZ = dg_sock::network_trivial_serializer::size(XAlignMetadata{});

        if (!stdxx::is_pow2(alignment))
        {
            throw std::bad_alloc();
        }

        const size_t max_fwd_sz = alignment + (METADATA_SZ - 1u);

        if (max_fwd_sz > std::numeric_limits<alignment_header_t>::max())
        {
            throw std::bad_alloc();
        }

        if (blk_sz == 0u)
        {
            return nullptr;
        }

        size_t adj_blk_sz   = blk_sz + max_fwd_sz;
        void * ptr          = allocation_resource_obj::get()->malloc(adj_blk_sz);

        if (ptr == nullptr)
        {
            std::abort();
        }

        void * aligned_ptr              = dg_sock::network_allocation::dg_align(std::next(static_cast<char *>(ptr), METADATA_SZ), alignment); //forward METADATA_SZ to reserve the METADATA_SZ, align the alignment (guaranteed to fit because we have extra ALIGMENT_SZ - 1u)
        alignment_header_t difference   = std::distance(static_cast<char *>(ptr), static_cast<char *>(aligned_ptr));
        void * metadata_header_addr     = std::prev(static_cast<char *>(aligned_ptr), METADATA_SZ);

        dg_sock::network_trivial_serializer::serialize_into(static_cast<char *>(metadata_header_addr), XAlignMetadata{.difference    = difference, 
                                                                                                                      .blk_sz        = stdxx::wrap_safe_integer_cast(blk_sz)});

        return aligned_ptr;
    }

    extern void dg_xaligned_free(void * ptr) noexcept
    {
        if (ptr == nullptr)
        {
            return;
        }

        constexpr size_t METADATA_SZ    = dg_sock::network_trivial_serializer::size(XAlignMetadata{});

        void * metadata_header_addr     = std::prev(static_cast<char *>(ptr), METADATA_SZ);
        auto metadata                   = XAlignMetadata{};
        dg_sock::network_trivial_serializer::deserialize_into(metadata, static_cast<const char *>(metadata_header_addr));
        void * org_ptr                  = std::prev(static_cast<char *>(ptr), metadata.difference); 

        allocation_resource_obj::get()->free(org_ptr); 
    }

    extern auto dg_xaligned_blk_size(void * ptr) noexcept -> size_t
    {
        if (ptr == nullptr)
        {
            return 0u;
        }

        constexpr size_t METADATA_SZ    = dg_sock::network_trivial_serializer::size(XAlignMetadata{});

        void * metadata_header_addr     = std::prev(static_cast<char *>(ptr), METADATA_SZ);
        auto metadata                   = XAlignMetadata{};
        dg_sock::network_trivial_serializer::deserialize_into(metadata, static_cast<const char *>(metadata_header_addr));

        return metadata.blk_sz;
    }

    template <class T, class ...Args>
    auto std_new_object(Args&& ...args) -> T *
    {
        static_assert(sizeof(T) != 0u);
        void * blk = nullptr;

        if constexpr(alignof(T) <= dg_sock::network_allocation::DEFAULT_ALIGNMENT_SZ)
        {
            blk = dg_sock::network_allocation::dg_malloc(sizeof(T));
        }
        else
        {
            blk = dg_sock::network_allocation::dg_aligned_alloc(alignof(T), sizeof(T));
        }

        if (blk == nullptr)
        {
            throw std::bad_alloc();
        }

        if constexpr(std::is_nothrow_constructible_v<T, Args&&...>)
        {
            return new (blk) T(std::forward<Args>(args)...);
        }
        else
        {
            try
            {
                return new (blk) T(std::forward<Args>(args)...);
            }
            catch (...)
            {
                if constexpr(alignof(T) <= dg_sock::network_allocation::DEFAULT_ALIGNMENT_SZ)
                {
                    dg_sock::network_allocation::dg_free(blk);
                }
                else
                {
                    dg_sock::network_allocation::dg_aligned_free(blk);
                }

                throw;
            }
        }
    }

    template <class = void>
    static inline constexpr bool FALSE_VAL = false;

    template <class T>
    auto std_delete_object(T * obj) noexcept
    {
        if constexpr(std::is_nothrow_destructible_v<T>)
        {
            std::destroy_at(obj);
        }
        else
        {
            try
            {
                std::destroy_at(obj);
            }
            catch (...)
            {
                std::abort();
            }
        }

        if constexpr(alignof(T) <= dg_sock::network_allocation::DEFAULT_ALIGNMENT_SZ)
        {
            [[clang::noinline]] dg_sock::network_allocation::dg_free(static_cast<void *>(obj));
        }
        else
        {
            [[clang::noinline]] dg_sock::network_allocation::dg_aligned_free(static_cast<void *>(obj));
        }
    }

    template <class T>
    auto std_new_array(size_t sz) -> T *
    {
        static_assert(sizeof(T) != 0u);

        if (sz == 0u)
        {
            return nullptr;
        }

        size_t allocation_blk_sz    = sz * sizeof(T); 
        void * blk                  = dg_sock::network_allocation::dg_xaligned_alloc(alignof(T), allocation_blk_sz);

        if (blk == nullptr)
        {
            throw std::bad_alloc();
        }  

        if constexpr(std::is_nothrow_default_constructible_v<T>)
        {
            return new (blk) T[sz];
        }
        else
        {
            try
            {
                return new (blk) T[sz];
            }
            catch (...)
            {
                dg_sock::network_allocation::dg_xaligned_free(blk);
                throw;
            }
        }
    }

    template <class T>
    void std_delete_array(T * arr) noexcept
    {
        if (arr == nullptr)
        {
            return;
        }

        size_t allocation_blk_sz    = dg_sock::network_allocation::dg_xaligned_blk_size(arr);
        size_t sz                   = allocation_blk_sz / sizeof(T);

        if constexpr(std::is_nothrow_destructible_v<T>)
        {
            std::destroy(arr, std::next(arr, sz));
        }
        else
        {
            try
            {
                std::destroy(arr, std::next(arr, sz));
            }
            catch (...)
            {
                std::abort();
            }
        }

        [[clang::noinline]] dg_sock::network_allocation::dg_xaligned_free(static_cast<void *>(arr));
    }

    // template <class T>
    // using NoExceptAllocator = std::allocator<T>;

    template <class T>
    class NoExceptAllocator
    {
        private:

            static consteval auto is_no_align_malloc() -> bool
            {
                return alignof(T) <= dg_sock::network_allocation::DEFAULT_ALIGNMENT_SZ;
            }

        public:

            using value_type                                = T;
            using pointer                                   = T*;
            using const_pointer                             = const T*;
            using reference                                 = T&;
            using const_reference                           = const T&;
            using size_type                                 = std::size_t;
            using difference_type                           = std::ptrdiff_t;
            using propagate_on_container_move_assignment    = std::true_type;
            using is_always_equal                           = std::true_type; // Stateless

            template <class U>
            struct rebind
            {
                using other = NoExceptAllocator<U>; 
            };

            constexpr NoExceptAllocator() noexcept = default;
            constexpr NoExceptAllocator(const NoExceptAllocator&) noexcept = default;
            
            template <class U>
            constexpr NoExceptAllocator(const NoExceptAllocator<U>&) noexcept {}
            
            ~NoExceptAllocator() = default;

            auto allocate(std::size_t n) -> pointer
            {
                if (allocation_resource_obj::get() == nullptr)
                {
                    std::abort();
                }

                if constexpr(is_no_align_malloc())
                {
                    return static_cast<pointer>(dg_sock::network_allocation::dg_malloc(sizeof(T) * n));
                }
                else
                {
                    return static_cast<pointer>(dg_sock::network_allocation::dg_aligned_alloc(alignof(T), sizeof(T) * n));
                }
            }

            void deallocate(T* p, std::size_t n) noexcept
            {
                if constexpr(is_no_align_malloc())
                {
                    [[clang::noinline]] dg_sock::network_allocation::dg_free(p);
                }
                else
                {
                    [[clang::noinline]] dg_sock::network_allocation::dg_aligned_free(p);
                }
            }
    };

    template <class T, class T1>
    constexpr auto operator==(const NoExceptAllocator<T>&, const NoExceptAllocator<T1>&) noexcept -> bool
    {
        return true;
    }

    template<class T, class T1>
    constexpr auto operator!=(const NoExceptAllocator<T>&, const NoExceptAllocator<T1>&) noexcept -> bool
    {
        return false;
    }

    template <class T, class = void>
    struct unique_ptr_chooser
    {
        using type  = std::unique_ptr<T, decltype(&std_delete_object<T>)>;
    };

    template <class T>
    struct unique_ptr_chooser<T, std::void_t<std::enable_if_t<std::is_array_v<T>, bool>>>: std::enable_if<std::is_unbounded_array_v<T>, std::unique_ptr<T, decltype(&std_delete_array<std::remove_extent_t<T>>)>>{};

    // template <class T>
    // using unique_ptr = typename unique_ptr_chooser<T>::type;

    template <class T>
    using shared_ptr = std::shared_ptr<T>;

    template <class T>
    using unique_ptr = shared_ptr<T>;

    template <class T, class ...Args>
    auto make_shared(Args&& ...args) -> std::shared_ptr<T>
    {
        return std::allocate_shared<T>(NoExceptAllocator<char>{}, std::forward<Args>(args)...);
    }

    template <class T, class ...Args>
    auto make_unique(Args&& ...args) -> unique_ptr<T> //people in the prophecy said that if we change unique_ptr -> shared_ptr most people wouldn't notice
    {
        return std::allocate_shared<T>(NoExceptAllocator<char>{}, std::forward<Args>(args)...);

        // if constexpr(std::is_array_v<T>)
        // {
        //     if constexpr(std::is_unbounded_array_v<T>)
        //     {
        //         using elemental_t = std::remove_extent_t<T>;
        //         return unique_ptr<T>(std_new_array<elemental_t>(std::forward<Args>(args)...), std_delete_array<elemental_t>);
        //     }
        //     else
        //     {
        //         static_assert(FALSE_VAL<>);
        //     }
        // }
        // else
        // {
        //     return unique_ptr<T>(std_new_object<T>(std::forward<Args>(args)...), std_delete_object<T>);
        // }
    }
}

namespace dg_sock
{
    template <class T>
    using unique_ptr = dg_sock::network_allocation::unique_ptr<T>;

    template <class T>
    using shared_ptr = dg_sock::network_allocation::shared_ptr<T>;

    template <class T, class ...Args>
    auto make_unique(Args&& ...args) -> unique_ptr<T>
    {
        return dg_sock::network_allocation::make_unique<T>(std::forward<Args>(args)...);
    }

    template <class T, class ...Args>
    auto make_shared(Args&& ...args) -> shared_ptr<T>
    {
        return dg_sock::network_allocation::make_shared<T>(std::forward<Args>(args)...);
    }
}

#endif
