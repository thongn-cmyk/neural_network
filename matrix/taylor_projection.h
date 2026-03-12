//HEADER_CONTROL 1

#ifndef __TAYLOR_PROJECTION_H__
#define __TAYLOR_PROJECTION_H__

#include <stdint.h>
#include <stdlib.h>
#include "stdx.h"
#include <type_traits>
#include <stdexcept>

namespace taylor_projection
{
    static inline constexpr size_t MAX_BASE_COEFFICIENT = 20u;
    static inline constexpr bool HAS_FAST_DIV           = true;

    template <class LhsFloatType, class RhsFloatType>
    constexpr auto fast_div(LhsFloatType lhs, RhsFloatType rhs) -> decltype(lhs / rhs)
    {
        static_assert(std::is_floating_point_v<LhsFloatType>);
        static_assert(std::is_floating_point_v<RhsFloatType>);

        if constexpr(HAS_FAST_DIV)
        {
            return lhs * (1 / rhs);
        }
        else
        {
            return lhs / rhs;
        }
    }

    constexpr auto factorial(size_t x) -> size_t
    {
        if (x == 0u)
        {
            return 1u;
        }

        return factorial(x - 1u) * x;
    }

    template <class FloatType, class PromotedFloatType = FloatType>
    constexpr auto radian_normalize(FloatType x,
                                    const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        return static_cast<FloatType>(std::remainder(static_cast<PromotedFloatType>(x), std::numbers::pi_v<PromotedFloatType> * 2));
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
    constexpr void taylor_radian_to_euclidean_space(const FloatType * radian_coeff_arr,
                                                    SzContainer coeff_arr_sz_container,
                                                    FloatType * euclid_coeff_arr,
                                                    const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        PromotedFloatType carry_multiplier = 1;

        for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
        {
            euclid_coeff_arr[i] = carry_multiplier * std::sin(static_cast<PromotedFloatType>(radian_coeff_arr[i]));
            carry_multiplier    *= std::cos(static_cast<PromotedFloatType>(radian_coeff_arr[i]));
        }
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    constexpr auto base_taylor_project(FloatType x,
                                       const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                       const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                                       const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if constexpr(HasBoundCheck)
        {
            if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT) [[unlikely]]
            {
                throw std::invalid_argument("bad base coefficient size, max reached");
            }
        }

        PromotedFloatType projected_result  = 0;
        PromotedFloatType x_multiplier      = 1;
        size_t factorial_denum              = 1;

        for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
        {
            PromotedFloatType delta_result  = fast_div(static_cast<PromotedFloatType>(coeff_arr[i]), static_cast<PromotedFloatType>(factorial_denum)) * x_multiplier;
            projected_result                += delta_result;
            x_multiplier                    *= x;
            factorial_denum                 *= i + 1;
        }

        return projected_result;
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
    constexpr auto taylor_project(FloatType x,
                                  const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                  const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return base_taylor_project(x, coeff_arr, coeff_arr_sz_container, promotion_tag, std::integral_constant<bool, true>{});
    }

    template <class FloatType, class BatchSizeContainer, class SzContainer, class PromotedFloatType, bool HasBoundCheck = false>
    constexpr void base_batch_taylor_project(const FloatType * x_arr, BatchSizeContainer x_arr_sz_container,
                                             const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                             PromotedFloatType * y_arr,
                                             const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        for (size_t i = 0u; i < x_arr_sz_container.get(); ++i)
        {
            y_arr[i] = base_taylor_project(x_arr[i], coeff_arr, coeff_arr_sz_container, stdx::Tag<PromotedFloatType>{}, bound_check);
        }
    }

    template <class FloatType, class BatchSizeContainer, class SzContainer, class PromotedFloatType>
    constexpr void batch_taylor_project(const FloatType * x_arr, BatchSizeContainer x_arr_sz_container,
                                        const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                        PromotedFloatType * y_arr)
    {
        return base_batch_taylor_project(x_arr, x_arr_sz_container,
                                         coeff_arr, coeff_arr_sz_container,
                                         y_arr,
                                         std::integral_constant<bool, true>{});
    }

    constexpr auto get_multivariate_taylor_projection_coefficient_size(size_t in_feature_sz, size_t base_coeff_sz) -> size_t
    {
        if (in_feature_sz == 0u)
        {
            throw std::invalid_argument("bad input feature size, 0");
        }

        if (base_coeff_sz > MAX_BASE_COEFFICIENT)
        {
            throw std::invalid_argument("bad base coefficient size, max reached");
        }

        return std::pow(base_coeff_sz, in_feature_sz);
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr void check_base_multivariate_taylor_project_arguments(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                    CoeffSizeContainer base_coeff_sz_container,
                                                                    const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                    const stdx::Tag<PromotedFloatType>& promotion_tag)
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (x_arr_sz_container.get() == 0u)
        {
            throw std::invalid_argument("bad input feature size, 0");
        }

        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            throw std::invalid_argument("bad base coefficient size, max reached");
        }

        size_t rem_coeff_sz = coeff_arr_cap - coeff_arr_offset;
        size_t required_sz  = get_multivariate_taylor_projection_coefficient_size(x_arr_sz_container.get(), base_coeff_sz_container.get());

        if (rem_coeff_sz < required_sz)
        {
            throw std::invalid_argument("insufficient remaining coefficient size");
        }
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    constexpr auto base_multivariate_taylor_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                    CoeffSizeContainer base_coeff_sz_container,
                                                    const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                    const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                                                    const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if constexpr(HasBoundCheck)
        {
            if (x_arr_sz_container.get() == 0u) [[unlikely]]
            {
                throw std::invalid_argument("bad input feature size, 0");
            }
        }

        if (x_arr_sz_container.get() == 1u)
        {
            const size_t tentative_nxt_offset = coeff_arr_offset + base_coeff_sz_container.get();

            if constexpr(HasBoundCheck)
            {
                if (tentative_nxt_offset > coeff_arr_cap) [[unlikely]]
                {
                    throw std::runtime_error("coefficient vector ran out of space");
                }
            }

            const FloatType * coeff_arr_arg = std::next(coeff_arr, coeff_arr_offset);
            coeff_arr_offset                = tentative_nxt_offset;

            return base_taylor_project(x_arr[0], coeff_arr_arg, base_coeff_sz_container, promotion_tag, bound_check);
        }

        PromotedFloatType projected_result  = 0;
        PromotedFloatType x_multiplier      = 1;
        size_t factorial_denum              = 1u;

        for (size_t i = 0u; i < base_coeff_sz_container.get(); ++i)
        {
            PromotedFloatType coeff         = base_multivariate_taylor_project(std::next(x_arr), stdx::safe_minus_one(x_arr_sz_container),
                                                                               base_coeff_sz_container,
                                                                               coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                               promotion_tag,
                                                                               bound_check);

            PromotedFloatType delta_result  = fast_div(coeff, static_cast<PromotedFloatType>(factorial_denum)) * x_multiplier;
            projected_result                += delta_result;
            x_multiplier                    *= x_arr[0];
            factorial_denum                 *= i + 1;
        }

        return projected_result;
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr auto multivariate_taylor_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                               CoeffSizeContainer base_coeff_sz_container,
                                               const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                               const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        check_base_multivariate_taylor_project_arguments(x_arr, x_arr_sz_container,
                                                         base_coeff_sz_container,
                                                         coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                         promotion_tag);

        return base_multivariate_taylor_project(x_arr, x_arr_sz_container,
                                                base_coeff_sz_container,
                                                coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                promotion_tag,
                                                std::integral_constant<bool, false>{});
    }

    constexpr auto get_multidimensional_taylor_projection_coefficient_size(size_t in_feature_sz,
                                                                           size_t base_coeff_sz,
                                                                           size_t out_feature_sz,
                                                                           bool has_logit_reuse) -> size_t
    {
        return get_multivariate_taylor_projection_coefficient_size(in_feature_sz, base_coeff_sz) * out_feature_sz;
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr __attribute__((noinline)) void multidimensional_taylor_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                             CoeffSizeContainer base_coeff_sz_container,
                                                                             const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                             FloatType * output_arr, size_t output_arr_sz,
                                                                             const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                                                                             bool has_logit_reuse_tag = true)
    {
        // const size_t saved_coeff_arr_offset = coeff_arr_offset;

        for (size_t i = 0u; i < output_arr_sz; ++i)
        {
            // if (has_logit_reuse_tag)
            // {
                // coeff_arr_offset = saved_coeff_arr_offset;
            // }

            output_arr[i]   = multivariate_taylor_project(x_arr, x_arr_sz_container,
                                                          base_coeff_sz_container,
                                                          coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                          promotion_tag);
        }
    }

    //this is actually hard!!
    //let's try to code this 10x faster

    //what do we learn from the heavily optimized code?

    //(1): memory fetch spent time / math instruction spent time
    //(2): heavily branch optimization by doing one single precond check before executing the function
    //(3): vectorization by using for loop and std::integral_constant<size_t, SZ>

    //memory fetch is expensive, even when they are in the L1 cache, we'd want to unfrequent the cache eviction by saturating the math instruction time at the molecule level
    //such is the base_batch_taylor_project, where it'd crunch so many operations that the memory fetch becomes irrelevant

    //we dont roll our own vectorized instructions, we'd want to hint the compiler by using __restrict__ and assume_aligned, though that is not used here
    //trust the compiler to do their optimization heuristics, we are only to provide the surrounding environment arguments

    //place __attribute__((noinline)) at a compute unit function, to not blow up the instruction cache which would reduce the program speed

    //i could not crunch this to the theoretical limit of 100 billion operations/ second, but the code is good enough for cuda + host, even though cuda requires a special local_memory to further crunch the operations

    constexpr auto get_batch_multivariate_taylor_projection_coefficient_size(size_t in_feature_sz,
                                                                             size_t base_coeff_sz,
                                                                             size_t batch_sz) -> size_t
    {
        if (in_feature_sz == 0u)
        {
            throw std::invalid_argument("bad input feature size, 0");
        }

        if (base_coeff_sz > MAX_BASE_COEFFICIENT)
        {
            throw std::invalid_argument("bad base coefficient size, max reached");
        }

        if (batch_sz == 0u)
        {
            throw std::invalid_argument("bad batch size, 0");
        }

        return std::pow(base_coeff_sz, in_feature_sz);
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType>
    constexpr void check_base_batch_multivariate_taylor_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                CoeffSizeContainer base_coeff_sz_container,
                                                                const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                PromotedFloatType * y_arr)
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (x_arr_sz_container.get() == 0u)
        {
            throw std::invalid_argument("bad input feature size, 0");
        }

        if (batch_sz_container.get() == 0u)
        {
            throw std::invalid_argument("bad batch size, 0");
        }

        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            throw std::invalid_argument("bad base coefficient size, max reached");
        }

        size_t rem_coeff_sz = coeff_arr_cap - coeff_arr_offset;
        size_t required_sz  = get_batch_multivariate_taylor_projection_coefficient_size(x_arr_sz_container.get(), base_coeff_sz_container.get(), batch_sz_container.get());

        if (rem_coeff_sz < required_sz)
        {
            throw std::invalid_argument("insufficient remaining coefficient size");
        }
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType, bool HasBoundCheck = true>
    constexpr void base_batch_multivariate_taylor_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                          CoeffSizeContainer base_coeff_sz_container,
                                                          const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                          PromotedFloatType * y_arr,
                                                          const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if constexpr(HasBoundCheck)
        {
            if (batch_sz_container.get() == 0u) [[unlikely]]
            {
                throw std::invalid_argument("bad batch size, 0");
            }

            if (x_arr_sz_container.get() == 0u) [[unlikely]]
            {
                throw std::invalid_argument("bad input feature size, 0");
            }
        }
        else
        {
            if (batch_sz_container.get() == 0u || x_arr_sz_container.get() == 0u)
            {
                std::unreachable();
            }
        }

        if (x_arr_sz_container.get() == 1u)
        {
            const size_t tentative_nxt_offset   = coeff_arr_offset + base_coeff_sz_container.get();

            if constexpr(HasBoundCheck)
            {
                if (tentative_nxt_offset > coeff_arr_cap) [[unlikely]]
                {
                    throw std::runtime_error("coefficient vector ran out of space");
                }
            }

            const FloatType * coeff_arr_arg     = std::next(coeff_arr, coeff_arr_offset);
            coeff_arr_offset                    = tentative_nxt_offset;

            base_batch_taylor_project(flat_x_arr_arr, batch_sz_container,
                                      coeff_arr_arg, base_coeff_sz_container,
                                      y_arr,
                                      bound_check);

            return;
        }

        size_t factorial_denum = 1u;

        alignas(alignof(std::max_align_t)) PromotedFloatType projected_arr[batch_sz_container.get()]; //I have a hinge that this can be optimized, let's try
        alignas(alignof(std::max_align_t)) PromotedFloatType x_multiplier_arr[batch_sz_container.get()];

        std::fill(x_multiplier_arr, std::next(x_multiplier_arr, batch_sz_container.get()), 1);
        std::fill(y_arr, std::next(y_arr, batch_sz_container.get()), 0);

        for (size_t i = 0u; i < base_coeff_sz_container.get(); ++i)
        {
            base_batch_multivariate_taylor_project(std::next(flat_x_arr_arr, batch_sz_container.get()), stdx::safe_minus_one(x_arr_sz_container), batch_sz_container,
                                                   base_coeff_sz_container,
                                                   coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                   projected_arr,
                                                   bound_check);

            for (size_t j = 0u; j < batch_sz_container.get(); ++j)
            {
                PromotedFloatType coeff         = projected_arr[j];
                PromotedFloatType delta_result  = fast_div(coeff, static_cast<PromotedFloatType>(factorial_denum)) * x_multiplier_arr[j];
                y_arr[j]                        += delta_result;
                x_multiplier_arr[j]             *= flat_x_arr_arr[j];
            }

            factorial_denum *= i + 1;
        }
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType>
    constexpr void batch_multivariate_taylor_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                     CoeffSizeContainer base_coeff_sz_container,
                                                     const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                     PromotedFloatType * y_arr)
    {
        check_base_batch_multivariate_taylor_project(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                                     base_coeff_sz_container,
                                                     coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                     y_arr);

        base_batch_multivariate_taylor_project(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                               base_coeff_sz_container,
                                               coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                               y_arr,
                                               std::integral_constant<bool, false>{});
    }

    constexpr auto get_batch_multidimensional_taylor_projection_coefficient_size(size_t in_feature_sz,
                                                                                 size_t batch_sz,
                                                                                 size_t base_coeff_sz,
                                                                                 size_t out_feature_sz,
                                                                                 bool has_logit_reuse) -> size_t
    {
        return get_batch_multivariate_taylor_projection_coefficient_size(in_feature_sz, base_coeff_sz, batch_sz) * out_feature_sz;
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr __attribute__((noinline)) void batch_multidimensional_taylor_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                   CoeffSizeContainer base_coeff_sz_container,
                                                                                   const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                   PromotedFloatType * flat_output_arr_arr, size_t output_dimension_sz,
                                                                                   bool has_logit_reuse_tag = true)
    {
        PromotedFloatType * it_flat_output_arr_arr = flat_output_arr_arr;

        for (size_t i = 0u; i < output_dimension_sz; ++i)
        {
            batch_multivariate_taylor_project(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                              base_coeff_sz_container,
                                              coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                              it_flat_output_arr_arr);

            std::advance(it_flat_output_arr_arr, batch_sz_container.get());
        }
    }
}

#endif