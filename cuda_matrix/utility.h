#ifndef __CUDA_MATRIX_TAG_H__
#define __CUDA_MATRIX_TAG_H__

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <type_traits>

namespace cuda_matrix::utility
{
    template <class T>
    struct Tag{};

    struct NormalSizeContainer
    {
        size_t value;

        constexpr __device__ NormalSizeContainer() noexcept = default;
        constexpr __device__ NormalSizeContainer(size_t value): value(value){}

        constexpr __device__ auto get() const noexcept -> size_t
        {
            return this->value;
        }
    };

    template <size_t SZ>
    struct IntegralSizeContainer
    {
        constexpr __device__ IntegralSizeContainer() = default;
        constexpr __device__ IntegralSizeContainer(std::integral_constant<size_t, SZ>) {}

        consteval __device__ auto get() -> size_t
        {
            return SZ;
        }
    };

    template <class T>
    constexpr __device__ auto safe_ptr_access(T * ptr) -> T *
    {
        if (ptr == nullptr)
        {
            assert(false);
        }

        return ptr;
    }

    constexpr __device__ auto unsigned_multiply(size_t a, size_t b, bool * overflow = nullptr) -> size_t
    {
        __uint128_t promoted = static_cast<__uint128_t>(a) * b;

        static_assert(sizeof(size_t) * 2u <= sizeof(__uint128_t));

        if (promoted > std::numeric_limits<size_t>::max() && overflow != nullptr)
        {
            *overflow = true;
        }

        return promoted;
    }

    constexpr __device__ auto unsigned_add(size_t a, size_t b, bool * overflow = nullptr) -> size_t
    {
        __uint128_t promoted = static_cast<__uint128_t>(a) + b;

        static_assert(sizeof(size_t) < sizeof(__uint128_t));
        
        if (promoted > std::numeric_limits<size_t>::max() && overflow != nullptr)
        {
            *overflow = true;
        }

        return promoted;
    }

    constexpr __device__ auto safe_non_zero_access(size_t sz) -> size_t
    {
        if (sz == 0u)
        {
            assert(false);
        }

        return sz;
    }

    constexpr __device__ auto unsigned_pow(size_t base, size_t exp, bool * overflow = nullptr) -> size_t
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

    constexpr __device__ auto to_size_container(size_t sz) -> NormalSizeContainer
    {
        return {sz};
    }

    template <size_t SZ>
    constexpr __device__ auto to_size_container(const std::integral_constant<size_t, SZ>) -> IntegralSizeContainer<SZ>
    {
        return {};
    }

    class GenericNaN
    {
        public:

            template <class FloatType, std::enable_if_t<std::is_floating_point_v<FloatType>, bool> = true>
            constexpr __device__ operator FloatType() const noexcept
            {
                return nanf("");
            }
    };

    constexpr __device__ auto generic_nan() -> GenericNaN
    {
        return {};
    }

}

#endif