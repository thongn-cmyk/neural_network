#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_UTILITY_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_UTILITY_H__

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <type_traits>
#include "assert.h"
#include <cuda_management/utility.h>
#include "local_exception.h"

namespace taylor_matrix::cuda_matrix::utility
{
    using namespace cuda_management::utility;

    template <class T>
    struct Tag{};

    struct NormalSizeContainer
    {
        size_t value;

        __device__ constexpr NormalSizeContainer() noexcept = default;
        __device__ constexpr NormalSizeContainer(size_t value): value(value){}

        __device__ constexpr auto get() const noexcept -> size_t
        {
            return this->value;
        }
    };

    template <size_t SZ>
    struct IntegralSizeContainer
    {
        __device__ constexpr IntegralSizeContainer() = default;
        __device__ constexpr IntegralSizeContainer(std::integral_constant<size_t, SZ>) {}

        consteval __device__ auto get() -> size_t
        {
            return SZ;
        }
    };

    __device__ constexpr auto unsigned_multiply(size_t a, size_t b, bool * overflow = nullptr) -> size_t
    {
        __uint128_t promoted = static_cast<__uint128_t>(a) * b;

        static_assert(sizeof(size_t) * 2u <= sizeof(__uint128_t));

        if (promoted > std::numeric_limits<size_t>::max() && overflow != nullptr)
        {
            *overflow = true;
        }

        return promoted;
    }

    __device__ constexpr auto unsigned_add(size_t a, size_t b, bool * overflow = nullptr) -> size_t
    {
        __uint128_t promoted = static_cast<__uint128_t>(a) + b;

        static_assert(sizeof(size_t) < sizeof(__uint128_t));
        
        if (promoted > std::numeric_limits<size_t>::max() && overflow != nullptr)
        {
            *overflow = true;
        }

        return promoted;
    }

    __device__ constexpr auto safe_non_zero_access(size_t sz) -> size_t
    {
        if (sz == 0u)
        {
            assert(false);
        }

        return sz;
    }

    __device__ constexpr auto unsigned_pow(size_t base, size_t exp, bool * overflow = nullptr) -> size_t
    {
        if (exp == 0u)
        {
            return 1u;
        }

        if (base == 0u)
        {
            return 0u;
        }

        size_t half_pow     = unsigned_pow(base, exp / 2u, overflow);
        size_t premod_pow   = unsigned_multiply(half_pow, half_pow, overflow);

        if (exp % 2u == 0u)
        {
            return premod_pow;
        }

        return unsigned_multiply(premod_pow, base, overflow);
    }

    __device__ constexpr auto to_size_container(size_t sz) -> NormalSizeContainer
    {
        return {sz};
    }

    template <size_t SZ>
    __device__ constexpr auto to_size_container(const std::integral_constant<size_t, SZ>) -> IntegralSizeContainer<SZ>
    {
        return {};
    }

    class GenericNaN
    {
        public:

            template <class FloatType, std::enable_if_t<std::is_floating_point_v<FloatType>, bool> = true>
            __device__ constexpr operator FloatType() const noexcept
            {
                return nanf("");
            }
    };

    __device__ constexpr auto generic_nan() -> GenericNaN
    {
        return {};
    }

    __device__ constexpr auto access_guard(size_t i, size_t sz) -> size_t
    {
        assert(i < sz);

        return i;
    }

    template <size_t FIRST, size_t LAST, class Callbackable>
    __device__ constexpr void to_constant_number(size_t value,
                                                 const std::integral_constant<size_t, FIRST>,
                                                 const std::integral_constant<size_t, LAST>,
                                                 Callbackable&& callbackable,
                                                 local_exception::local_exception_t * err = nullptr)
    {
        static_assert(LAST > FIRST);

        constexpr size_t RANGE  = LAST - FIRST;
        bool was_thru           = false;

        [&]<size_t ...IDX>(const std::index_sequence<IDX...>)
        {
            (
                [&]
                {
                    (void) IDX;
                    constexpr size_t i = FIRST + IDX;

                    if (i == value)
                    {
                        callbackable(std::integral_constant<size_t, i>{});
                        was_thru = true;
                    }
                }(), ...
            );
        }(std::make_index_sequence<RANGE>{});

        if (!was_thru)
        {
            if (err != nullptr)
            {
                *err = local_exception::OTHER_INVALID_ARGUMENT_CODE;
            }
        }
    }

    template <class T, std::enable_if_t<std::is_same_v<T, float>, bool> = true>
    __device__ constexpr auto abs(T x) -> T
    {
        return fabsf(x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, double>, bool> = true>
    __device__ constexpr auto abs(T x) -> T
    {
        return fabs(x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, float>, bool> = true>
    __device__ constexpr auto dg_copysign(T x, T sign) -> T
    {
        return copysignf(x, sign);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, double>, bool> = true>
    __device__ constexpr auto dg_copysign(T x, T sign) -> T
    {
        return copysign(x, sign);
    }
}

#endif