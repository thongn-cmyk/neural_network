//in this special chapter of allocator, we'd want to focus on one very special allocator

//trapping rain allocator

//specializing in trapping upround bitset
//specializing in trapping exact allocation

//and release the allocation once the threshold has been reached

//the base allocation would be scope allocator, where we'd <in_scope> and <outscope> the stacks
//or best yet, we'd call the malloc directly

//i dont have a lot of time to play around, in this implementation, we'd work on numerical precision, cuda + parallel map_reduce architecture and randomization algorithm
//we'd focus on the basis of approximation-completeness and the virtues of the encoding methods
//it turns out that taylor approximation of 4-5-6 dimensions of 8! is sufficient because 8! can approx synth waves very good

//explaining the concept of approximation completeness and focal is hard to grasp, but I'll try my best to describe the algorithm by using induction and best choices
//we are seeing an algorithm that exceeds 1 billion people intellect combined if trained appropriately
//but we'd use this for financial purposes only, for the philosophy of life is to not be explored

#ifndef __CONVENTIONAL_ALLOCATOR_H__
#define __CONVENTIONAL_ALLOCATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include "unordered_node_map.h"
#include "assert.h"
#include <optional>
#include <functional>
#include <utility>
#include <algorithm>
#include "stdx.h"
#include <memory>
#include "assert.h"

namespace conventional_allocator
{
    class AllocatorInterface
    {
        public:

            virtual ~AllocatorInterface() = default;

            virtual auto malloc(size_t sz) -> std::add_pointer_t<char> = 0;
            virtual void free(void * buf) noexcept = 0;
    };

    class ScopeInterface
    {
        public:

            virtual ~ScopeInterface() = default;

            virtual void in_scope() = 0;
            virtual void out_scope() noexcept = 0;
    };

    class ScopeAllocatorInterface: public virtual AllocatorInterface,
                                   public virtual ScopeInterface{};

    class TrappingRainAllocatorInterface: public virtual AllocatorInterface
    {
        public:

            virtual void flush() noexcept = 0;
    };

    class StdAllocator : public virtual AllocatorInterface
    {
        public:

            auto malloc(size_t sz) -> char *
            {
                if (sz == 0u)
                {
                    return nullptr;
                }

                return new char[sz];
            }

            void free(void * buf) noexcept
            {
                if (buf == nullptr)
                {
                    return;
                }

                delete[] static_cast<char *>(buf);
            }
    };

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
    __attribute__((noinline)) auto object_malloc(AllocatorInterface& allocator,
                                                 size_t object_count) -> char *
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

        char * buf                  = allocator.malloc(aligned_total_sz);
        char * fwd_buf              = std::next(buf, sizeof(uint16_t));
        char * aligned_fwd_buf      = reinterpret_cast<char *>(align(reinterpret_cast<uintptr_t>(fwd_buf), std::integral_constant<uintptr_t, alignof(T)>{}));

        char * prev_aligned_fwd_buf = std::prev(aligned_fwd_buf, sizeof(uint16_t));
        uint16_t dist               = std::distance(fwd_buf, aligned_fwd_buf);

        std::memcpy(prev_aligned_fwd_buf, &dist, sizeof(uint16_t));

        return aligned_fwd_buf;
    }

    __attribute__((noinline)) void object_free(AllocatorInterface& allocator,
                                               void * buf) noexcept
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

        allocator.free(org_buf);
    }

    class ScopeGuard
    {
        private:

            ScopeInterface * volatile scope;

        public:

            __attribute__((always_inline)) ScopeGuard(ScopeInterface * scope): scope(scope)
            {
                stdx::safe_ptr_access(this->scope)->in_scope();
            }

            __attribute__((always_inline)) ~ScopeGuard() noexcept
            {
                this->scope->out_scope();
            }
    };

    template <class T>
    class STLCompatibleAllocator
    {
        private:

            std::shared_ptr<AllocatorInterface> allocator;

            template <class U>
            friend class STLCompatibleAllocator;

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
                using other = STLCompatibleAllocator<U>;
            };

            constexpr STLCompatibleAllocator(): allocator(nullptr){}

            constexpr STLCompatibleAllocator(std::shared_ptr<AllocatorInterface> allocator): allocator(std::move(allocator)){}

            template <class U>
            constexpr STLCompatibleAllocator(const STLCompatibleAllocator<U>& other): allocator(other.allocator){}

            constexpr auto allocate(size_t n) -> T *
            {
                stdx::safe_ptr_access(this->allocator.get());
                return static_cast<T *>(static_cast<void *>(object_malloc<T>(*this->allocator, n)));
            }

            constexpr void deallocate(T * ptr, size_t sz)
            {
                stdx::safe_ptr_access(this->allocator.get());
                object_free(*this->allocator, static_cast<void *>(ptr));
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

    template <class T, class U>
    constexpr auto operator ==(const STLCompatibleAllocator<T>& lhs, const STLCompatibleAllocator<U>& rhs) noexcept -> bool
    {
        return true;
    }

    template <class T, class U>
    constexpr auto operator !=(const STLCompatibleAllocator<T>& lhs, const STLCompatibleAllocator<U>& rhs) noexcept -> bool
    {
        return false;
    }

    class NoCopyConstructorBase
    {
        public:

            NoCopyConstructorBase() = default;
            NoCopyConstructorBase(const NoCopyConstructorBase&) = delete;
    };

    class NoMoveConstructorBase
    {
        public:

            NoMoveConstructorBase() = default;
            NoMoveConstructorBase(NoMoveConstructorBase&&) = delete;
    };

    class NoCopyAssignableBase
    {
        public:

            NoCopyAssignableBase() = default;
            NoCopyAssignableBase& operator =(const NoCopyAssignableBase&) = delete;
    };

    class NoMoveAssignableBase
    {
        public:

            NoMoveAssignableBase() = default;
            NoMoveAssignableBase& operator =(NoMoveAssignableBase&&) = delete;
    };

    class UniqueInstantiationBase: public NoCopyConstructorBase,
                                   public NoMoveConstructorBase,
                                   public NoCopyAssignableBase,
                                   public NoMoveAssignableBase{
        
        public:

            UniqueInstantiationBase() = default;
    };

    class ScopeAllocator : public virtual ScopeAllocatorInterface,
                           private UniqueInstantiationBase
    {
        private:

            struct MemoryPiece
            {
                char * buf;
                size_t buf_sz;
            };

            struct SavedPoint
            {
                size_t mempiece_idx;
                size_t mempiece_offset;
            };

            std::shared_ptr<AllocatorInterface> base_allocator;

            stdx::transparent_vector<MemoryPiece, STLCompatibleAllocator<char>> mempiece_vec;
            stdx::transparent_vector<std::optional<SavedPoint>, STLCompatibleAllocator<char>> saved_point_vec;

            size_t base_allocation_sz;
            std::optional<SavedPoint> current_point;

        public:

            ScopeAllocator(std::shared_ptr<AllocatorInterface> base_allocator,
                           size_t base_allocation_sz): base_allocator(base_allocator),
                                                       mempiece_vec(STLCompatibleAllocator<char>(base_allocator)),
                                                       saved_point_vec(STLCompatibleAllocator<char>(base_allocator)),
                                                       base_allocation_sz(base_allocation_sz),
                                                       current_point(std::nullopt)
            {
                if (this->base_allocator == nullptr)
                {
                    throw std::invalid_argument("bad base allocator, null");
                }

                if (this->base_allocation_sz == 0u)
                {
                    throw std::invalid_argument("bad base allocation size, 0");
                }
            }

            ~ScopeAllocator() noexcept
            {
                for (auto& mempiece: mempiece_vec)
                {
                    this->base_allocator->free(mempiece.buf);
                }
            }

            auto malloc(size_t sz) -> char *
            {
                if (this->saved_point_vec.empty())
                {
                    throw std::runtime_error("base 0 stack_idx");
                }

                if (!this->current_point.has_value())
                {
                    // throw std::runtime_error("internal corruption");
                    std::abort();
                }

                SavedPoint valid_point = this->current_point.value();

                while (true)
                {
                    size_t rem_sz = this->mempiece_vec[valid_point.mempiece_idx].buf_sz - valid_point.mempiece_offset;

                    if (rem_sz >= sz)
                    {
                        char * rs           = std::next(this->mempiece_vec[valid_point.mempiece_idx].buf, valid_point.mempiece_offset);
                        this->current_point = SavedPoint{.mempiece_idx      = valid_point.mempiece_idx,
                                                         .mempiece_offset   = valid_point.mempiece_offset + sz};

                        return rs;
                    }

                    this->reserve_mempiece_vec_for(valid_point.mempiece_idx + 2u);
                    valid_point = SavedPoint{.mempiece_idx      = valid_point.mempiece_idx + 1u,
                                             .mempiece_offset   = 0u};
                }
            }

            void free(void * buf) noexcept
            {
                (void) buf;
            }

            void in_scope()
            {
                if (!this->current_point.has_value())
                {
                    this->reserve_mempiece_vec_for(1u);
                    this->saved_point_vec.push_back(this->current_point);

                    this->current_point = SavedPoint{.mempiece_idx      = 0u,
                                                     .mempiece_offset   = 0u};

                    return;
                }

                this->saved_point_vec.push_back(this->current_point);
            }

            void out_scope() noexcept
            {
                if (this->saved_point_vec.empty())
                {
                    // throw std::runtime_error("negative stack collapse");
                    std::abort();
                }

                std::optional<SavedPoint> saved_point   = this->saved_point_vec.back();
                this->saved_point_vec.pop_back();
                this->current_point                     = saved_point;
            }

        private:

            auto get_pow2(size_t idx) -> size_t
            {
                return size_t{1} << idx;
            }

            auto get_mempiece_buffer_capacity_at(size_t idx) -> size_t
            {
                return this->get_pow2(idx) * this->base_allocation_sz;
            }

            void reserve_mempiece_vec_for(size_t sz)
            {
                size_t desired_sz   = std::max(sz, static_cast<size_t>(this->mempiece_vec.size()));
                size_t diff_sz      = desired_sz - this->mempiece_vec.size();

                for (size_t i = 0u; i < diff_sz; ++i)
                {
                    size_t offset   = this->mempiece_vec.size() + i;
                    size_t buf_sz   = this->get_mempiece_buffer_capacity_at(offset);
                    char * buf      = this->base_allocator->malloc(buf_sz);

                    try
                    {
                        this->mempiece_vec.push_back(MemoryPiece{.buf       = buf,
                                                                 .buf_sz    = buf_sz});
                    }
                    catch (...)
                    {
                        this->base_allocator->free(buf);
                        throw;
                    }
                }
            }
    };

    template <class Key, class Value, class Allocator>
    using internal_unordered_map = unordered_map_variants::unordered_node_map<Key, Value, size_t, std::integral_constant<bool, true>, std::hash<Key>, std::equal_to<Key>, typename std::allocator_traits<Allocator>::template rebind_alloc<std::pair<const Key, Value>>>;

    class ExactReuseAllocator: public virtual TrappingRainAllocatorInterface,
                               private UniqueInstantiationBase
    {
        private:

            std::shared_ptr<AllocatorInterface> base;
            internal_unordered_map<size_t, stdx::transparent_vector<char *, STLCompatibleAllocator<char>>, STLCompatibleAllocator<char>> reuse_map;
            size_t reuse_map_global_cap;
            size_t reuse_map_elemental_cap;
            size_t reuse_map_elemental_sz;

        public:

            ExactReuseAllocator(std::shared_ptr<AllocatorInterface> persistent_base,
                                std::shared_ptr<AllocatorInterface> loose_base,
                                size_t reuse_map_global_cap,
                                size_t reuse_map_elemental_cap): base(loose_base),
                                                                 reuse_map(STLCompatibleAllocator<char>(persistent_base)),
                                                                 reuse_map_global_cap(reuse_map_global_cap),
                                                                 reuse_map_elemental_cap(reuse_map_elemental_cap),
                                                                 reuse_map_elemental_sz(0u)
            {                                        
                if (this->base == nullptr)
                {
                    throw std::invalid_argument("bad base, null");
                }

                if (this->reuse_map_global_cap == 0u)
                {
                    throw std::invalid_argument("bad reuse map global cap, 0");
                }

                if (this->reuse_map_elemental_cap == 0u)
                {
                    throw std::invalid_argument("bad reuse map elemental cap, 0");
                }
            }

            ~ExactReuseAllocator() noexcept
            {
                this->internal_flush();
            }

            auto malloc(size_t sz) -> char *
            {
                if (sz == 0u)
                {
                    return nullptr;
                }

                if (auto map_ptr = this->reuse_map.find(sz); map_ptr != this->reuse_map.end())
                {
                    if (!map_ptr->second.empty())
                    {
                        char * rs = map_ptr->second.back();
                        map_ptr->second.pop_back();

                        return rs;
                    }
                }

                char * rs = this->base->malloc(sz + sizeof(size_t));
                std::memcpy(rs, &sz, sizeof(size_t));

                return std::next(rs, sizeof(size_t));
            }

            __attribute__((noipa, noinline)) void free(void * buf) noexcept
            {
                if (buf == nullptr)
                {
                    return;
                }

                void * previous_buf = std::prev(static_cast<char *>(buf), sizeof(size_t));
                size_t sz;
                std::memcpy(&sz, previous_buf, sizeof(size_t));

                if (this->reuse_map_elemental_sz == this->reuse_map_elemental_cap) [[unlikely]]
                {
                    this->internal_flush();
                }

                auto map_ptr = this->reuse_map.find(sz);

                if (map_ptr == this->reuse_map.end())
                {
                    if (this->reuse_map.size() == this->reuse_map_global_cap) [[unlikely]]
                    {
                        this->internal_flush();
                    }

                    try
                    {
                        auto [new_ptr, status] = this->reuse_map.insert({sz, stdx::transparent_vector<char *, STLCompatibleAllocator<char>>(this->base)});
                        assert(status);
                        map_ptr = new_ptr;
                    }
                    catch (...)
                    {
                        this->base->free(previous_buf);
                        return;
                    }
                }

                try
                {
                    map_ptr->second.push_back(static_cast<char *>(buf));
                    this->reuse_map_elemental_sz += 1u;
                }
                catch (...)
                {
                    this->base->free(previous_buf);
                    return;
                }
            }

            void flush() noexcept
            {
                this->internal_flush();
            }

        private:

            static constexpr auto to_user_pointer(void * buf) noexcept  -> void *
            {
                return std::next(static_cast<char *>(buf), sizeof(size_t));
            }

            static constexpr auto to_internal_pointer(void * buf) noexcept -> void *
            {
                return std::prev(static_cast<char *>(buf), sizeof(size_t));
            }

            __attribute__((noinline)) void internal_flush() noexcept
            {
                for (auto& pair: this->reuse_map)
                {
                    for (auto& buf: pair.second)
                    {
                        this->base->free(to_internal_pointer(buf));
                    }
                }

                this->reuse_map.clear();
                this->reuse_map_elemental_sz = 0u;
            }
    };

    class ScopeExactReuseAllocator: public virtual ScopeAllocatorInterface,
                                    private UniqueInstantiationBase
    {
        private:

            std::shared_ptr<ScopeAllocator> scope_allocator;
            std::shared_ptr<ExactReuseAllocator> reuse_allocator;

        public:

            ScopeExactReuseAllocator(std::shared_ptr<AllocatorInterface> base_allocator,
                                     size_t scope_allocator_base_allocation_sz,
                                     size_t reuse_map_global_cap,
                                     size_t reuse_map_elemental_cap)
            {
                this->scope_allocator   = std::allocate_shared<ScopeAllocator>(STLCompatibleAllocator<char>(base_allocator),
                                                                               base_allocator,
                                                                               scope_allocator_base_allocation_sz);

                this->reuse_allocator   = std::allocate_shared<ExactReuseAllocator>(STLCompatibleAllocator<char>(base_allocator),
                                                                                    base_allocator,
                                                                                    this->scope_allocator,
                                                                                    reuse_map_global_cap,
                                                                                    reuse_map_elemental_cap);
            }

            auto malloc(size_t sz) -> char *
            {
                return this->reuse_allocator->malloc(sz);
            }

            void free(void * buf) noexcept
            {
                this->reuse_allocator->free(buf);
            }

            void in_scope()
            {
                this->scope_allocator->in_scope();
            }

            void out_scope() noexcept
            {
                {
                    this->reuse_allocator->flush();
                }

                {
                    this->scope_allocator->out_scope();
                }
            }
    };
}

#endif