#ifndef __CUDA_MANAGEMENT_UTILITY_H__
#define __CUDA_MANAGEMENT_UTILITY_H__

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <type_traits>
#include "assert.h"
#include <bit>
#include <device_functions.h>
#include <cuda.h>
#include "cuda_runtime.h"

namespace cuda_management::utility
{
    template <class T>
    struct Tag{};

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    __device__ inline auto ulog2(T val) noexcept -> size_t
    {
        using promoted_value                = long long int; 
        using counterpart_promoted_value    = uint64_t;

        if (val == 0u)
        {
            assert(false);
        }

        static_assert(sizeof(T) <= sizeof(counterpart_promoted_value));

        return (sizeof(promoted_value) * CHAR_BIT - 1u) - __clzll(std::bit_cast<promoted_value>(static_cast<counterpart_promoted_value>(val)));
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    __device__ constexpr auto ceil2(T val) noexcept -> T
    {
        if (val < 2u) [[unlikely]]
        {
            return 1u;
        }
        else [[likely]]
        {
            T uplog_value = ulog2(static_cast<T>(val - 1u)) + 1u;
            return T{1u} << uplog_value;
        }
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    __device__ constexpr auto is_pow2(T value) -> bool
    {
        return value != 0u && (value & static_cast<T>(value - 1)) == 0u;
    }

    template <class T>
    __device__ constexpr auto safe_ptr_access(T * ptr) -> T *
    {
        if (ptr == nullptr)
        {
            assert(false);
        }

        return ptr;
    }

    template <class T1, class T>
    __device__ constexpr auto safe_integer_cast(T value) noexcept -> T1{

        static_assert(std::numeric_limits<T>::is_integer);
        static_assert(std::numeric_limits<T1>::is_integer);

        if constexpr(std::is_unsigned_v<T> && std::is_unsigned_v<T1>)
        {
            (void) value;
        }
        else if constexpr(std::is_signed_v<T> && std::is_signed_v<T1>)
        {
            (void) value;
        }
        else
        {
            if constexpr(std::is_signed_v<T>)
            {
                if constexpr(sizeof(T) > sizeof(T1))
                {
                    (void) value;
                }
                else
                {
                    if (value < 0)
                    {
                        assert(false);
                    }
                    else
                    {
                        return value; //sizeof(signed) <= sizeof(unsigned)
                    }
                }
            }
            else
            {
                if constexpr(sizeof(T1) > sizeof(T))
                {
                    (void) value;
                }
                else
                {
                    if (value > std::numeric_limits<T1>::max())
                    {
                        assert(false);
                    }
                    else
                    {
                        return value; //sizeof(unsigned) >= sizeof(signed)
                    }
                }
            }
        }

        if (value > std::numeric_limits<T1>::max())
        {
            assert(false);
        }

        if (value < std::numeric_limits<T1>::min())
        {
            assert(false);
        }

        return value;
    }

    template <class T>
    struct safe_integer_cast_wrapper
    {
        static_assert(std::numeric_limits<T>::is_integer);
        T value;

        template <class U>
        __device__ constexpr operator U() const noexcept
        {
            return safe_integer_cast<U>(this->value);
        }
    };

    template <class T>
    __device__ constexpr auto wrap_safe_integer_cast(T value) noexcept
    {
        return safe_integer_cast_wrapper<T>{value};
    }

    template <class Lhs, class Rhs, class = void>
    struct can_add: std::false_type{};

    template <class Lhs, class Rhs>
    struct can_add<Lhs, Rhs, std::void_t<decltype(std::declval<Lhs>() + std::declval<Rhs>())>>: std::true_type{};

    template <class Lhs, class Rhs>
    __device__ static inline constexpr bool can_add_v = can_add<Lhs, Rhs>::value;

    template <class Lhs, class Rhs, class = void>
    struct can_sub: std::false_type{};

    template <class Lhs, class Rhs>
    struct can_sub<Lhs, Rhs, std::void_t<decltype(std::declval<Lhs>() - std::declval<Rhs>())>>: std::true_type{};

    template <class Lhs, class Rhs>
    __device__ static inline constexpr bool can_sub_v = can_sub<Lhs, Rhs>::value;

    template <class RandomAccessIterator, std::enable_if_t<can_add_v<RandomAccessIterator&, intmax_t&>, bool> = true>
    __device__ constexpr auto next(RandomAccessIterator it, intmax_t sz = 1) -> RandomAccessIterator
    {
        return it + sz;
    }

    template <class RandomAccessIterator, std::enable_if_t<can_sub_v<RandomAccessIterator&, intmax_t&>, bool> = true>
    __device__ constexpr auto prev(RandomAccessIterator it, intmax_t sz = 1) -> RandomAccessIterator
    {
        return it - sz;
    }

    template <class Iterator>
    __device__ constexpr void advance(Iterator& it, intmax_t displacement)
    {
        it = next(it, displacement);
    }

    template <class Iterator, class ValueLike>
    __device__ constexpr void fill(Iterator first, Iterator last, ValueLike&& value)
    {
        while (first != last)
        {
            *first = value;
            advance(first, 1);
        }
    }

    template <class T, std::enable_if_t<std::numeric_limits<T>::is_integer, bool> = true>
    __device__ constexpr auto clamp(T val, T min_val, T max_val) -> T
    {
        if (val < min_val)
        {
            return min_val;
        }

        if (val > max_val)
        {
            return max_val;
        }

        return val;
    }

    template <class T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
    __device__ constexpr auto clamp(T val, T min_val, T max_val) -> T
    {
        if (isnan(val))
        {
            return min_val;
        }

        if (val < min_val)
        {
            return min_val;
        }

        if (val > max_val)
        {
            return max_val;
        }

        return val;
    }

    template <class RandomAccessIterator, std::enable_if_t<can_sub_v<RandomAccessIterator&, RandomAccessIterator&>, bool> = true>
    __device__ constexpr auto distance(RandomAccessIterator first, RandomAccessIterator last) -> intmax_t
    {
        return last - first;
    }

    template <class T, class T_Like>
    __device__ constexpr auto exchange(T& value, T_Like&& new_value) -> T
    {
        static_assert(std::is_trivial_v<std::decay_t<T>>);

        T old_value = value;
        value = std::forward<T_Like>(new_value);

        return old_value;
    }

    template <class T>
    __device__ constexpr void swap(T& lhs, T& rhs) noexcept(std::conjunction_v<std::is_nothrow_move_constructible<T>,
                                                                               std::is_nothrow_move_assignable<T>>)
    {
        T tmp   = std::move(lhs);
        lhs     = std::move(rhs);
        rhs     = std::move(tmp);
    }

    template <class T>
    __device__ constexpr auto max(T lhs, T rhs) -> T
    {
        if (lhs > rhs)
        {
            return lhs;
        }
        
        return rhs;
    }

    template <class T>
    __device__ constexpr auto min(T lhs, T rhs) -> T
    {
        if (lhs < rhs)
        {
            return lhs;
        }

        return rhs;
    }

    template <class T>
    __device__ constexpr void destroy_at(T * obj)
    {
        if (obj == nullptr)
        {
            return;
        }

        obj->~T();
    }

    template <class T>
    __device__ constexpr void destroy(T * first, T * last)
    {
        while (first != last)
        {
            first->~T();
            advance(first, 1u);
        }
    }

    template <class FromIterator, class ToIterator>
    __device__ constexpr auto copy(FromIterator first, FromIterator last,
                                   ToIterator dst) -> ToIterator
    {
        while (first != last)
        {
            *dst = *first;

            advance(first, 1);
            advance(dst, 1);
        }

        return dst;
    }
}

#endif