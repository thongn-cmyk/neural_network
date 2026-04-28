#ifndef __CUDA_MATRIX_TAYLOR_PROJECTION_H__
#define __CUDA_MATRIX_TAYLOR_PROJECTION_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include <type_traits>
#include "assert.h"
#include "space_operation.h"

namespace cuda_matrix::taylor_projection
{
    using namespace cuda_matrix::utility;

    //when I worked on the thesis of "perfect space" and the perfect next projection

    //we could think of this as a 2 dimensional + 2 dimensional -> 1 dimensional projection
    //the 1 dimensional projection must be able to touch all the required set of points (says that there are 10 points, 2 points, 1 point, etc.)
    //the hinge here is that it must be able to constructively touch the points, such that when an addition is performed, destructive interferences are minimized in the case 

    //addition in the case, if with independent logits for each of the base operation, then the amount of addition operations is irrelevant in our searching mission
    //because addiition does not pollute the searching semantic space, just make it dumber because we are on the same level of semantic space, so there is no sound waves -> words -> semantic meaning -> etc.

    //that means, we must increase the operating dimension to be able to duplicate the inputs, such is that when an addition operation is performed, we are adding more to the semantic space to fulfill the touch all the required set of points with sufficient size

    //I also have thought long and hard about what that means if we have the same window for input and output, and it turns out that such is not important to have dynamic sizes, because the output entropy is lesser than the input entropy, and we almost always want to increase the space size compared to the most compact form of input

    //what I have been unable to do is to limit the search spaces
    //if we look at the shape_projection, the search space is limited to the base dimension of 10 dimensions shrinkable to 1 dimensions with just a few twists of spaces 
    //yet the shape projection is missing a multiplicative factor, such is required that we recursively pairwise multiply the shape projection space with another rotating space to limit the operating dimensions, otherwise it would be incredibly hard to search in such a vast space 
    //we'd work on the experimental research later 

    //in other words, the effort we spent in searching in the lower power spaces must be expressed in the formula 

    static inline constexpr __device__ size_t MAX_BASE_COEFFICIENT = 20u;
    static inline constexpr __device__ bool HAS_FAST_DIV           = true;

    template <class LhsFloatType, class RhsFloatType>
    static constexpr __device__ auto fast_div(LhsFloatType lhs, RhsFloatType rhs) -> decltype(lhs / rhs)
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

    static constexpr __device__ auto factorial(size_t x) -> size_t
    {
        if (x == 0u)
        {
            return 1u;
        }

        return factorial(x - 1u) * x;
    }

    static constexpr __device__ auto safe_minus_one(const NormalSizeContainer& sz) -> NormalSizeContainer
    {
        if (sz.get() == 0u)
        {
            return NormalSizeContainer(0u);
        }

        return NormalSizeContainer(sz.get() - 1u);
    }

    template <size_t SZ>
    static constexpr __device__ auto safe_minus_one(const IntegralSizeContainer<SZ>& sz)
    {
        if constexpr(SZ == 0u)
        {
            return IntegralSizeContainer<SZ>{};
        }
        else
        {
            return IntegralSizeContainer<SZ - 1u>{};
        }
    }

    //------------------------ Taylor Projection -----------------------

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
    constexpr __device__ auto check_base_taylor_project_arguments(FloatType x,
                                                                  const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                                                  const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> bool
    {
        if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return false;
        }

        return true;
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    static constexpr __device__ auto base_taylor_project(FloatType x,
                                                         const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                                         const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                         const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if constexpr(HasBoundCheck)
        {
            if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT) [[unlikely]]
            {
                assert(false);
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
    constexpr __device__ auto taylor_project(FloatType x,
                                             const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                             const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return base_taylor_project(x,
                                   coeff_arr, coeff_arr_sz_container,
                                   promotion_tag,
                                   std::integral_constant<bool, true>{});
    }

    //------------------------- Batch Taylor Projection -----------------------

    template <class FloatType, class BatchSizeContainer, class SzContainer, class PromotedFloatType>
    constexpr __device__ auto check_base_batch_taylor_project_arguments(const FloatType * x_arr, BatchSizeContainer x_arr_sz_container,
                                                                        const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                                                        PromotedFloatType * y_arr) -> bool
    {
        if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return false;
        }

        return true;
    } 

    template <class FloatType, class BatchSizeContainer, class SzContainer, class PromotedFloatType, bool HasBoundCheck = false>
    static constexpr __device__ void base_batch_taylor_project(const FloatType * x_arr, BatchSizeContainer x_arr_sz_container,
                                                               const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                                               PromotedFloatType * y_arr,
                                                               const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        for (size_t i = 0u; i < x_arr_sz_container.get(); ++i)
        {
            y_arr[i] = base_taylor_project(x_arr[i],
                                           coeff_arr, coeff_arr_sz_container,
                                           Tag<PromotedFloatType>{},
                                           bound_check);
        }
    }

    template <class FloatType, class BatchSizeContainer, class SzContainer, class PromotedFloatType>
    constexpr __device__ void batch_taylor_project(const FloatType * x_arr, BatchSizeContainer x_arr_sz_container,
                                                   const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                                   PromotedFloatType * y_arr)
    {
        return base_batch_taylor_project(x_arr, x_arr_sz_container,
                                         coeff_arr, coeff_arr_sz_container,
                                         y_arr,
                                         std::integral_constant<bool, true>{});
    }

    //------------------------ Multivariate Taylor Projection -----------------------

    constexpr __device__ auto get_multivariate_taylor_projection_coefficient_size(size_t in_feature_sz,
                                                                                  size_t base_coeff_sz,
                                                                                  bool * overflow = nullptr) -> size_t
    {
        return unsigned_pow(base_coeff_sz, in_feature_sz, overflow);
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr __device__ auto check_base_multivariate_taylor_project_arguments(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                               CoeffSizeContainer base_coeff_sz_container,
                                                                               const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                               const Tag<PromotedFloatType>& promotion_tag) -> bool
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return false;
        }

        size_t rem_coeff_sz = coeff_arr_cap - coeff_arr_offset;
        bool overflow       = false;  
        size_t required_sz  = get_multivariate_taylor_projection_coefficient_size(x_arr_sz_container.get(),
                                                                                  base_coeff_sz_container.get(),
                                                                                  &overflow);

        if (overflow)
        {
            return false;
        }

        if (rem_coeff_sz < required_sz)
        {
            return false;
        }

        return true;
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    static constexpr __device__ auto base_multivariate_taylor_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                      CoeffSizeContainer base_coeff_sz_container,
                                                                      const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                      const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                                      const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (x_arr_sz_container.get() == 0u) [[unlikely]]
        {
            return 0;
        }

        if (x_arr_sz_container.get() == 1u)
        {
            const size_t tentative_nxt_offset = coeff_arr_offset + base_coeff_sz_container.get();

            if constexpr(HasBoundCheck)
            {
                if (tentative_nxt_offset > coeff_arr_cap) [[unlikely]]
                {
                    assert(false);
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
            PromotedFloatType coeff         = base_multivariate_taylor_project(std::next(x_arr), safe_minus_one(x_arr_sz_container),
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
    constexpr __device__ auto multivariate_taylor_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                          CoeffSizeContainer base_coeff_sz_container,
                                                          const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                          const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        assert(check_base_multivariate_taylor_project_arguments(x_arr, x_arr_sz_container,
                                                                base_coeff_sz_container,
                                                                coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                promotion_tag));

        return base_multivariate_taylor_project(x_arr, x_arr_sz_container,
                                                base_coeff_sz_container,
                                                coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                promotion_tag,
                                                std::integral_constant<bool, false>{});
    }

    //------------------------ Multidimensional Taylor Projection -----------------------

    constexpr __device__ auto get_multidimensional_taylor_projection_coefficient_size(size_t in_feature_sz,
                                                                                      size_t base_coeff_sz,
                                                                                      size_t out_feature_sz,
                                                                                      bool has_logit_reuse_tag = true,
                                                                                      bool * overflow = nullptr) -> size_t
    {
        return unsigned_multiply(get_multivariate_taylor_projection_coefficient_size(in_feature_sz, base_coeff_sz, overflow),
                                 out_feature_sz,
                                 overflow);
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr __device__ auto check_multidimensional_taylor_project_arguments(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                              CoeffSizeContainer base_coeff_sz_container,
                                                                              const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                              FloatType * output_arr, size_t output_arr_sz,
                                                                              const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                                              bool has_logit_reuse_tag = true) -> bool
    {
        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return false;
        }

        bool overflow       = false;
        size_t rem_coeff_sz = coeff_arr_cap - coeff_arr_offset;
        size_t required_sz  = get_multidimensional_taylor_projection_coefficient_size(x_arr_sz_container.get(),
                                                                                      base_coeff_sz_container.get(),
                                                                                      output_arr_sz,
                                                                                      has_logit_reuse_tag,
                                                                                      &overflow);

        if (overflow)
        {
            return false;
        }

        if (rem_coeff_sz < required_sz)
        {
            return false;
        }

        return true;
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr __device__ __attribute__((noinline)) void multidimensional_taylor_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                                        CoeffSizeContainer base_coeff_sz_container,
                                                                                        const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                        FloatType * output_arr, size_t output_arr_sz,
                                                                                        const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                                                        bool has_logit_reuse_tag = true)
    {
        for (size_t i = 0u; i < output_arr_sz; ++i)
        {
            output_arr[i]   = multivariate_taylor_project(x_arr, x_arr_sz_container,
                                                          base_coeff_sz_container,
                                                          coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                          promotion_tag);
        }
    }

    //------------------------ Batch Multivariate Taylor Projection -----------------------

    constexpr __device__ auto get_batch_multivariate_taylor_projection_coefficient_size(size_t in_feature_sz,
                                                                                        size_t base_coeff_sz,
                                                                                        size_t batch_sz,
                                                                                        bool * overflow = nullptr) -> size_t
    {
        return unsigned_pow(base_coeff_sz, in_feature_sz, overflow);
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType>
    constexpr __device__ auto check_base_batch_multivariate_taylor_project_arguments(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                     CoeffSizeContainer base_coeff_sz_container,
                                                                                     const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                     PromotedFloatType * y_arr) -> bool
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return false;
        }

        bool overflow       = false;
        size_t rem_coeff_sz = coeff_arr_cap - coeff_arr_offset;
        size_t required_sz  = get_batch_multivariate_taylor_projection_coefficient_size(x_arr_sz_container.get(),
                                                                                        base_coeff_sz_container.get(),
                                                                                        batch_sz_container.get(),
                                                                                        &overflow);

        if (overflow)
        {
            return false;
        }

        if (rem_coeff_sz < required_sz)
        {
            return false;
        }

        return true;
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType, bool HasBoundCheck = true>
    static constexpr __device__ void base_batch_multivariate_taylor_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                            CoeffSizeContainer base_coeff_sz_container,
                                                                            const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                            PromotedFloatType * y_arr,
                                                                            const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (x_arr_sz_container.get() == 0u) [[unlikely]]
        {
            return;
        }

        if (x_arr_sz_container.get() == 1u)
        {
            const size_t tentative_nxt_offset   = coeff_arr_offset + base_coeff_sz_container.get();

            if constexpr(HasBoundCheck)
            {
                if (tentative_nxt_offset > coeff_arr_cap) [[unlikely]]
                {
                    assert(false);
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

        PromotedFloatType projected_arr[batch_sz_container.get()];
        PromotedFloatType x_multiplier_arr[batch_sz_container.get()];

        std::fill(x_multiplier_arr, std::next(x_multiplier_arr, batch_sz_container.get()), 1);
        std::fill(y_arr, std::next(y_arr, batch_sz_container.get()), 0);

        for (size_t i = 0u; i < base_coeff_sz_container.get(); ++i)
        {
            base_batch_multivariate_taylor_project(std::next(flat_x_arr_arr, batch_sz_container.get()), safe_minus_one(x_arr_sz_container), batch_sz_container,
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
    constexpr __device__ __attribute__((noinline)) void batch_multivariate_taylor_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                          CoeffSizeContainer base_coeff_sz_container,
                                                                                          const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                          PromotedFloatType * y_arr)
    {
        assert(check_base_batch_multivariate_taylor_project_arguments(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                                                      base_coeff_sz_container,
                                                                      coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                      y_arr));

        base_batch_multivariate_taylor_project(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                               base_coeff_sz_container,
                                               coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                               y_arr,
                                               std::integral_constant<bool, false>{});
    }

    //------------------------ Batch Multidimensional Taylor Projection -----------------------
    
    constexpr __device__ auto get_batch_multidimensional_taylor_projection_coefficient_size(size_t in_feature_sz,
                                                                                            size_t batch_sz,
                                                                                            size_t base_coeff_sz,
                                                                                            size_t out_feature_sz,
                                                                                            bool  has_logit_reuse_tag = true,
                                                                                            bool * overflow = nullptr) -> size_t
    {
        return unsigned_multiply(get_batch_multivariate_taylor_projection_coefficient_size(in_feature_sz, base_coeff_sz, batch_sz, overflow),
                                 out_feature_sz,
                                 overflow);
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr __device__ auto check_batch_multidimensional_taylor_project_arguments(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                    CoeffSizeContainer base_coeff_sz_container,
                                                                                    const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                    FloatType * flat_output_arr_arr, size_t output_dimension_sz,
                                                                                    const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                                                    bool has_logit_reuse_tag = true) -> bool
    {
        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return false;
        }

        bool overflow       = false;
        size_t rem_coeff_sz = coeff_arr_cap - coeff_arr_offset;
        size_t required_sz  = get_batch_multidimensional_taylor_projection_coefficient_size(x_arr_sz_container.get(),
                                                                                            batch_sz_container.get(),
                                                                                            base_coeff_sz_container.get(),
                                                                                            output_dimension_sz,
                                                                                            has_logit_reuse_tag,
                                                                                            &overflow);

        if (overflow)
        {
            return false;
        }

        if (rem_coeff_sz < required_sz)
        {
            return false;
        }

        return true;
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr __device__ __attribute__((noinline)) void batch_multidimensional_taylor_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
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