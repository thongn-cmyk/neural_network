#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_SHAPE_PROJECTION_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_SHAPE_PROJECTION_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include <type_traits>
#include "assert.h"
#include "space_operation.h"
#include <utility>
#include "local_exception.h"

namespace taylor_matrix::cuda_matrix::shape_projection
{
    using namespace taylor_matrix::cuda_matrix::utility;
    using namespace taylor_matrix::cuda_matrix::local_exception;

    //keep in mind that every decision made thus far is correct decision, from fixed tensor box => search => etc.
    //it's hard to keep things "not circular" in C++
    //and the model is entropy-rule compliant

    static inline __device__ constexpr size_t MAX_BASE_COEFFICIENT  = 20;
    static inline __device__ constexpr double HORIZONTAL_SCALER     = 4;
    static inline __device__ constexpr bool HAS_FAST_DIV            = true;

    template <class LhsFloatType, class RhsFloatType>
    __device__ static constexpr auto fast_div(LhsFloatType lhs, RhsFloatType rhs) -> decltype(lhs / rhs)
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

    __device__ static constexpr auto safe_minus_one(const NormalSizeContainer& sz) -> NormalSizeContainer
    {
        if (sz.get() == 0u)
        {
            return NormalSizeContainer(0u);
        }

        return NormalSizeContainer(sz.get() - 1u);
    }

    template <size_t SZ>
    __device__ static constexpr auto safe_minus_one(const IntegralSizeContainer<SZ>& sz)
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

    template <class FloatType, class PromotedFloatType = FloatType>
    __device__ static constexpr auto radian_normalize(FloatType x,
                                                      const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> FloatType
    {
        return taylor_matrix::cuda_matrix::space_operation::radian_normalize(x, Tag<PromotedFloatType>{});
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
    __device__ static constexpr void taylor_radian_to_euclidean_space(const FloatType * radian_coeff_arr,
                                                                      SzContainer coeff_arr_sz_container,
                                                                      FloatType * euclid_coeff_arr,
                                                                      const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
        {
            euclid_coeff_arr[i] = radian_coeff_arr[i];
        }

    //     PromotedFloatType carry_multiplier = 1u;

    //     for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
    //     {
    //         euclid_coeff_arr[i] = carry_multiplier * std::cos(static_cast<PromotedFloatType>(radian_coeff_arr[i]));
    //         carry_multiplier    *= std::sin(static_cast<PromotedFloatType>(radian_coeff_arr[i]));
    //     }

    //     for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
    //     {
    //         euclid_coeff_arr[i] *= (radian_coeff_arr[coeff_arr_sz_container.get() - 1u] + 1);
    //     }
    }

    //------------------------ Raw Taylor Projection -----------------------

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    __device__ static constexpr auto base_taylor_raw_shape_project(FloatType x,
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
            factorial_denum                 *= HORIZONTAL_SCALER;
        }

        return projected_result;
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
    __device__ static constexpr auto taylor_raw_shape_project(FloatType x,
                                                              const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                                              const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return base_taylor_raw_shape_project(x,
                                             coeff_arr, coeff_arr_sz_container,
                                             promotion_tag,
                                             std::integral_constant<bool, true>{});
    }

    //------------------------- Batch Taylor Projection -----------------------

    template <class FloatType, class BatchSizeContainer, class SzContainer, class PromotedFloatType, bool HasBoundCheck = false>
    __device__ static constexpr void base_batch_taylor_raw_shape_project(const FloatType * x_arr, BatchSizeContainer x_arr_sz_container,
                                                                         const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                                                         PromotedFloatType * y_arr,
                                                                         const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        for (size_t i = 0u; i < x_arr_sz_container.get(); ++i)
        {
            y_arr[i] = base_taylor_raw_shape_project(x_arr[i],
                                                     coeff_arr, coeff_arr_sz_container,
                                                     Tag<PromotedFloatType>{},
                                                     bound_check);
        }
    }

    //------------------------ Taylor Shape Projection -----------------------

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    __device__ static constexpr auto base_taylor_shape_project(FloatType x,
                                                               const FloatType * radian_coeff_arr, SzContainer coeff_arr_sz_container,
                                                               const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                               const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
    {
        if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT) [[unlikely]]
        {
            assert(false);
        }

        FloatType euclidean_coeff_space[MAX_BASE_COEFFICIENT];
        taylor_radian_to_euclidean_space(radian_coeff_arr,
                                         coeff_arr_sz_container,
                                         euclidean_coeff_space,
                                         promotion_tag);

        return base_taylor_raw_shape_project(x,
                                             euclidean_coeff_space, coeff_arr_sz_container,
                                             promotion_tag,
                                             bound_check);
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
    __device__ static constexpr auto taylor_shape_project(FloatType x,
                                                          const FloatType * radian_coeff_arr, SzContainer coeff_arr_sz_container,
                                                          const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return base_taylor_shape_project(x,
                                         radian_coeff_arr, coeff_arr_sz_container,
                                         promotion_tag,
                                         std::integral_constant<bool, true>{});
    }

    //------------------------- Batch Taylor Shape Projection -----------------------

    template <class FloatType, class BatchSizeContainer, class SzContainer, class PromotedFloatType, bool HasBoundCheck = false>
    __device__ static constexpr void base_batch_taylor_shape_project(const FloatType * x_arr, BatchSizeContainer x_arr_sz_container,
                                                                     const FloatType * radian_coeff_arr, SzContainer coeff_arr_sz_container,
                                                                     PromotedFloatType * y_arr,
                                                                     const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            assert(false);
        }

        FloatType euclidean_coeff_space[MAX_BASE_COEFFICIENT];
        taylor_radian_to_euclidean_space(radian_coeff_arr,
                                         coeff_arr_sz_container,
                                         euclidean_coeff_space,
                                         Tag<PromotedFloatType>{});

        base_batch_taylor_raw_shape_project(x_arr, x_arr_sz_container,
                                            euclidean_coeff_space, coeff_arr_sz_container,
                                            y_arr,
                                            bound_check);
    }

    //------------------------ Multivariate Taylor Shape Projection -----------------------

    __device__ constexpr auto get_multivariate_taylor_shape_projection_coefficient_size(size_t in_feature_sz,
                                                                                        size_t base_coeff_sz,
                                                                                        bool * overflow = nullptr) -> size_t
    {
        if (in_feature_sz == 0u)
        {
            return 0u;
        }

        return unsigned_pow(base_coeff_sz, in_feature_sz, overflow);
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType>
    __device__ static constexpr auto check_base_multivariate_taylor_shape_project_arguments(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                                            CoeffSizeContainer base_coeff_sz_container,
                                                                                            const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                            const Tag<PromotedFloatType>& promotion_tag) -> local_exception_t
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }

        bool overflow       = false;
        size_t rem_coeff_sz = coeff_arr_cap - coeff_arr_offset;
        size_t required_sz  = get_multivariate_taylor_shape_projection_coefficient_size(x_arr_sz_container.get(),
                                                                                        base_coeff_sz_container.get(),
                                                                                        &overflow);

        if (overflow)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }

        if (rem_coeff_sz < required_sz)
        {
            return INSUFFICIENT_LOGIT_VEC_SIZE_CODE;
        }

        return SUCCESS;
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    __device__ static constexpr auto base_multivariate_taylor_shape_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
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

            const FloatType * coeff_arr_arg = utility::next(coeff_arr, coeff_arr_offset);
            coeff_arr_offset                = tentative_nxt_offset;

            return base_taylor_shape_project(x_arr[0],
                                             coeff_arr_arg, base_coeff_sz_container,
                                             promotion_tag,
                                             bound_check);
        }

        FloatType projecting_coeff_arr[MAX_BASE_COEFFICIENT]; 

        for (size_t i = 0u; i < base_coeff_sz_container.get(); ++i)
        {
             projecting_coeff_arr[i] = base_multivariate_taylor_shape_project(utility::next(x_arr), safe_minus_one(x_arr_sz_container),
                                                                              base_coeff_sz_container,
                                                                              coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                              promotion_tag,
                                                                              bound_check);
        }

        return base_taylor_shape_project(x_arr[0],
                                         projecting_coeff_arr, base_coeff_sz_container,
                                         promotion_tag,
                                         bound_check);
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    __device__ constexpr auto multivariate_taylor_shape_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                CoeffSizeContainer base_coeff_sz_container,
                                                                const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                local_exception_t * err = nullptr,
                                                                const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        local_exception_t local_err = check_base_multivariate_taylor_shape_project_arguments(x_arr, x_arr_sz_container,
                                                                                             base_coeff_sz_container,
                                                                                             coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                             promotion_tag);

        if (local_err != SUCCESS)
        {
            if (err != nullptr)
            {
                *err = local_err;
            }

            return {};
        }

        return base_multivariate_taylor_shape_project(x_arr, x_arr_sz_container,
                                                      base_coeff_sz_container,
                                                      coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                      promotion_tag,
                                                      std::integral_constant<bool, false>{});
    }

    //------------------------ Multidimensional Taylor Shape Projection -----------------------

    __device__ constexpr auto get_multidimensional_taylor_shape_projection_coefficient_size(size_t in_feature_sz,
                                                                                            size_t base_coeff_sz,
                                                                                            size_t out_feature_sz,
                                                                                            bool has_logit_reuse_tag = true,
                                                                                            bool * overflow = nullptr) -> size_t
    {
        return unsigned_multiply(get_multivariate_taylor_shape_projection_coefficient_size(in_feature_sz, base_coeff_sz, overflow),
                                 out_feature_sz,
                                 overflow);
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    __device__ static constexpr auto check_multidimensional_taylor_shape_project_arguments(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                                           CoeffSizeContainer base_coeff_sz_container,
                                                                                           const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                           FloatType * output_arr, size_t output_arr_sz,
                                                                                           const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                                                           bool has_logit_reuse_tag = true) -> local_exception_t
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }

        bool overflow       = false;
        size_t rem_coeff_sz = coeff_arr_cap - coeff_arr_offset;
        size_t required_sz  = get_multidimensional_taylor_shape_projection_coefficient_size(x_arr_sz_container.get(),
                                                                                            base_coeff_sz_container.get(),
                                                                                            output_arr_sz,
                                                                                            has_logit_reuse_tag,
                                                                                            &overflow);

        if (overflow)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }

        if (rem_coeff_sz < required_sz)
        {
            return INSUFFICIENT_LOGIT_VEC_SIZE_CODE;
        }

        return SUCCESS;
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    __device__ constexpr __attribute__((noinline)) void multidimensional_taylor_shape_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                                              CoeffSizeContainer base_coeff_sz_container,
                                                                                              const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                              FloatType * output_arr, size_t output_arr_sz,
                                                                                              local_exception_t * err = nullptr,
                                                                                              const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                                                              bool has_logit_reuse_tag = true)
    {
        local_exception_t local_err = check_multidimensional_taylor_shape_project_arguments(x_arr, x_arr_sz_container,
                                                                                            base_coeff_sz_container,
                                                                                            coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                            output_arr, output_arr_sz,
                                                                                            promotion_tag,
                                                                                            has_logit_reuse_tag);

        if (local_err != SUCCESS)
        {
            if (err != nullptr)
            {
                *err = local_err;
            }

            return;
        }

        for (size_t i = 0u; i < output_arr_sz; ++i)
        {
            output_arr[i]   = multivariate_taylor_shape_project(x_arr, x_arr_sz_container,
                                                                base_coeff_sz_container,
                                                                coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                nullptr,
                                                                promotion_tag);
        }
    }

    //------------------------ Batch Multivariate Taylor Shape Projection -----------------------

    __device__ constexpr auto get_batch_multivariate_taylor_shape_projection_coefficient_size(size_t in_feature_sz,
                                                                                              size_t base_coeff_sz,
                                                                                              size_t batch_sz,
                                                                                              bool * overflow = nullptr) -> size_t
    {
        if (batch_sz == 0u)
        {
            return 0u;
        }

        if (in_feature_sz == 0u)
        {
            return 0u;
        }

        return unsigned_pow(base_coeff_sz, in_feature_sz, overflow);
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType>
    __device__ constexpr auto check_base_batch_multivariate_taylor_shape_project_arguments(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                           CoeffSizeContainer base_coeff_sz_container,
                                                                                           const FloatType * radian_coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                           PromotedFloatType * y_arr) -> local_exception_t
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }

        bool overflow       = false;
        size_t rem_coeff_sz = coeff_arr_cap - coeff_arr_offset;
        size_t required_sz  = get_batch_multivariate_taylor_shape_projection_coefficient_size(x_arr_sz_container.get(),
                                                                                              base_coeff_sz_container.get(),
                                                                                              batch_sz_container.get(),
                                                                                              &overflow);

        if (overflow)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }

        if (rem_coeff_sz < required_sz)
        {
            return INSUFFICIENT_LOGIT_VEC_SIZE_CODE;
        }

        return SUCCESS;
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType, bool HasBoundCheck = true>
    __device__ static constexpr void base_batch_multivariate_taylor_shape_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                  CoeffSizeContainer base_coeff_sz_container,
                                                                                  const FloatType * radian_coeff_arr, size_t& radian_coeff_arr_offset, size_t radian_coeff_arr_cap,
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
            const size_t tentative_nxt_offset       = radian_coeff_arr_offset + ((batch_sz_container.get() == 0u) ? size_t{0u}
                                                                                                                  : static_cast<size_t>(base_coeff_sz_container.get()));

            if constexpr(HasBoundCheck)
            {
                if (tentative_nxt_offset > radian_coeff_arr_cap) [[unlikely]]
                {
                    assert(false);
                }
            }

            const FloatType * radian_coeff_arr_arg  = utility::next(radian_coeff_arr, radian_coeff_arr_offset);
            radian_coeff_arr_offset                 = tentative_nxt_offset;

            base_batch_taylor_shape_project(flat_x_arr_arr, batch_sz_container,
                                            radian_coeff_arr_arg, base_coeff_sz_container,
                                            y_arr,
                                            bound_check);

            return;
        }

        PromotedFloatType projected_arr[batch_sz_container.get()]{};
        PromotedFloatType projecting_coeff_2d_arr[batch_sz_container.get() * base_coeff_sz_container.get()]{};

        for (size_t i = 0u; i < base_coeff_sz_container.get(); ++i)
        {
            base_batch_multivariate_taylor_shape_project(utility::next(flat_x_arr_arr, batch_sz_container.get()), safe_minus_one(x_arr_sz_container), batch_sz_container,
                                                         base_coeff_sz_container,
                                                         radian_coeff_arr, radian_coeff_arr_offset, radian_coeff_arr_cap,
                                                         projected_arr,
                                                         bound_check);

            for (size_t j = 0u; j < batch_sz_container.get(); ++j)
            {
                size_t virtual_idx                      = j * base_coeff_sz_container.get() + i;
                projecting_coeff_2d_arr[virtual_idx]    = projected_arr[j];
            }
        }

        for (size_t i = 0u; i < batch_sz_container.get(); ++i)
        {
            size_t first                                = i * base_coeff_sz_container.get();
            PromotedFloatType * projecting_coeff_arr    = utility::next(projecting_coeff_2d_arr, first);  

            y_arr[i]                                    = base_taylor_shape_project(flat_x_arr_arr[i],
                                                                                    projecting_coeff_arr, base_coeff_sz_container,
                                                                                    Tag<PromotedFloatType>{},
                                                                                    bound_check);
        }
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType>
    __device__ constexpr __attribute__((noinline)) void batch_multivariate_taylor_shape_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                                CoeffSizeContainer base_coeff_sz_container,
                                                                                                const FloatType * radian_coeff_arr, size_t& radian_coeff_arr_offset, size_t radian_coeff_arr_cap,
                                                                                                PromotedFloatType * y_arr,
                                                                                                local_exception_t * err = nullptr)
    {
        local_exception_t local_err = check_base_batch_multivariate_taylor_shape_project_arguments(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                                                                                   base_coeff_sz_container,
                                                                                                   radian_coeff_arr, radian_coeff_arr_offset, radian_coeff_arr_cap,
                                                                                                   y_arr);

        if (local_err != SUCCESS)
        {
            if (err != nullptr)
            {
                *err = local_err;
            }

            return;
        }

        base_batch_multivariate_taylor_shape_project(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                                     base_coeff_sz_container,
                                                     radian_coeff_arr, radian_coeff_arr_offset, radian_coeff_arr_cap,
                                                     y_arr,
                                                     std::integral_constant<bool, false>{});
    }

    //------------------------ Batch Multidimensional Taylor Shape Projection -----------------------

    __device__ constexpr auto get_batch_multidimensional_taylor_shape_projection_coefficient_size(size_t in_feature_sz,
                                                                                                  size_t batch_sz,
                                                                                                  size_t base_coeff_sz,
                                                                                                  size_t out_feature_sz,
                                                                                                  bool has_logit_reuse_tag = true,
                                                                                                  bool * overflow = nullptr) -> size_t
    {
        return unsigned_multiply(get_batch_multivariate_taylor_shape_projection_coefficient_size(in_feature_sz, base_coeff_sz, batch_sz, overflow),
                                 out_feature_sz,
                                 overflow);
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    __device__ constexpr auto check_batch_multidimensional_taylor_shape_project_argument(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                         CoeffSizeContainer base_coeff_sz_container,
                                                                                         const FloatType * radian_coeff_arr, size_t& radian_coeff_arr_offset, size_t radian_coeff_arr_cap,
                                                                                         PromotedFloatType * flat_output_arr_arr, size_t output_dimension_sz,
                                                                                         bool has_logit_reuse_tag = true) -> local_exception_t
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if (base_coeff_sz_container.get() > MAX_BASE_COEFFICIENT)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }

        bool overflow       = false;
        size_t rem_coeff_sz = radian_coeff_arr_cap - radian_coeff_arr_offset;
        size_t required_sz  = get_batch_multidimensional_taylor_shape_projection_coefficient_size(x_arr_sz_container.get(),
                                                                                                  batch_sz_container.get(),
                                                                                                  base_coeff_sz_container.get(),
                                                                                                  output_dimension_sz,
                                                                                                  has_logit_reuse_tag,
                                                                                                  &overflow);

        if (overflow)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }

        if (rem_coeff_sz < required_sz)
        {
            return INSUFFICIENT_LOGIT_VEC_SIZE_CODE;
        }

        return SUCCESS;
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    __device__ constexpr __attribute__((noinline)) void batch_multidimensional_taylor_shape_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                                    CoeffSizeContainer base_coeff_sz_container,
                                                                                                    const FloatType * radian_coeff_arr, size_t& radian_coeff_arr_offset, size_t radian_coeff_arr_cap,
                                                                                                    PromotedFloatType * flat_output_arr_arr, size_t output_dimension_sz,
                                                                                                    local_exception_t * err = nullptr,
                                                                                                    bool has_logit_reuse_tag = true)
    {
        local_exception_t local_err = check_batch_multidimensional_taylor_shape_project_argument(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                                                                                 base_coeff_sz_container,
                                                                                                 radian_coeff_arr, radian_coeff_arr_offset, radian_coeff_arr_cap,
                                                                                                 flat_output_arr_arr, output_dimension_sz,
                                                                                                 has_logit_reuse_tag);

        if (local_err != SUCCESS)
        {
            if (err != nullptr)
            {
                *err = local_err;
            }

            return;
        }

        PromotedFloatType * it_flat_output_arr_arr = flat_output_arr_arr;

        for (size_t i = 0u; i < output_dimension_sz; ++i)
        {
            batch_multivariate_taylor_shape_project(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                                    base_coeff_sz_container,
                                                    radian_coeff_arr, radian_coeff_arr_offset, radian_coeff_arr_cap,
                                                    it_flat_output_arr_arr);

            utility::advance(it_flat_output_arr_arr, batch_sz_container.get());
        }
    }
}

#endif