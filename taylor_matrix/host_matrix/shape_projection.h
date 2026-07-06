//HEADER_CONTROL 2

#ifndef __TAYLOR_MATRIX_HOST_MATRIX_SHAPE_PROJECTION_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_SHAPE_PROJECTION_H__

#include <stl_extension/stdx.h>
#include <type_traits>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <bit>

namespace taylor_matrix::host_matrix::shape_projection
{
    //the number one reason why we are copying and pasting is because this continuous space is subjected to a lot of changes
    //we dont know if the radian space + cosine space is sufficient for complex shape projection

    //what we reckoned is that spline interpolation is mandatory, and the fit 1-2 points happen almost immediately
    //regardless of the number of points
    //the larger powers is for the re-iteration of the region, such is to fit the curve into the projection space, by elaborating with other coefficients

    //what we have been trying to do is to cap the transformation at x^1, such is linear, this is for the better of the transformation
    //because we won't be exploding the powers of the transformation

    static inline constexpr size_t MAX_BASE_COEFFICIENT = 20;
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

    template <class T, std::enable_if_t<std::is_same_v<T, float>, bool> = true>
    constexpr auto fast_approx_one_half(T x) -> T
    {
        float abs_x     = std::abs(x);
        uint32_t u_val  = std::bit_cast<uint32_t>(abs_x);
        u_val           = (u_val >> 1) + 0x1FC00000;

        return std::copysign(std::bit_cast<float>(u_val), x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, float>, bool> = true>
    constexpr auto fast_approx_three_quarters(T x) -> T
    {
        float abs_x     = std::abs(x);
        uint32_t u_val  = std::bit_cast<uint32_t>(abs_x);
        u_val           = u_val - (u_val >> 2) + 0x0FE00000;

        return std::copysign(std::bit_cast<float>(u_val), x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, double>, bool> = true>
    constexpr auto fast_approx_three_quarters(T x) -> T
    {
        double abs_x    = std::abs(x);
        uint64_t u_val  = std::bit_cast<uint64_t>(abs_x);
        u_val           = u_val - (u_val >> 2) + 0x0FFC000000000000ULL;

        return std::copysign(std::bit_cast<double>(u_val), x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, float>, bool> = true>
    constexpr auto fast_approx_power_7_8(T x) -> T
    {
        float abs_x    = std::abs(x);
        uint32_t u_val = std::bit_cast<uint32_t>(abs_x);
        u_val          = u_val - (u_val >> 3) + 0x07F00000;

        return std::copysign(std::bit_cast<float>(u_val), x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, double>, bool> = true>
    constexpr auto fast_approx_power_7_8(T x) -> T
    {
        double abs_x   = std::abs(x);
        uint64_t u_val = std::bit_cast<uint64_t>(abs_x);
        u_val          = u_val - (u_val >> 3) + 0x07FE000000000000ULL;

        return std::copysign(std::bit_cast<double>(u_val), x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, float>, bool> = true>
    constexpr auto fast_approx_power_15_16(T x) -> T
    {
        float abs_x    = std::abs(x);
        uint32_t u_val = std::bit_cast<uint32_t>(abs_x);
        
        // Scale exponent by 15/16 using shifts: 1 - 1/16
        // Add the strictly calculated 32-bit magic constant
        u_val          = u_val - (u_val >> 4) + 0x03F80000;

        return std::copysign(std::bit_cast<float>(u_val), x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, double>, bool> = true>
    constexpr auto fast_approx_power_15_16(T x) -> T
    {
        double abs_x   = std::abs(x);
        uint64_t u_val = std::bit_cast<uint64_t>(abs_x);
        
        // Scale exponent by 15/16 using shifts: 1 - 1/16
        // Add the strictly calculated 64-bit magic constant
        u_val          = u_val - (u_val >> 4) + 0x03FF000000000000ULL;

        return std::copysign(std::bit_cast<double>(u_val), x);
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    constexpr auto base_taylor_raw_shape_project(FloatType x,
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
        PromotedFloatType x_multiplier      = fast_approx_one_half(x);

        for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
        {
            if (i == 0u)
            {
                projected_result    += coeff_arr[i];
            }
            else
            {
                projected_result    += static_cast<PromotedFloatType>(coeff_arr[i]) * x_multiplier;
                x_multiplier        = fast_approx_one_half(x_multiplier);
            }
        }

        return projected_result;
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
    constexpr auto taylor_raw_shape_project(FloatType x,
                                            const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                            const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return base_taylor_raw_shape_project(x, coeff_arr, coeff_arr_sz_container, promotion_tag, std::integral_constant<bool, true>{});
    }

    template <class FloatType, class BatchSizeContainer, class SzContainer, class PromotedFloatType, bool HasBoundCheck = false>
    constexpr void base_batch_taylor_raw_shape_project(const FloatType * x_arr, BatchSizeContainer x_arr_sz_container,
                                                       const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                                                       PromotedFloatType * y_arr,
                                                       const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        for (size_t i = 0u; i < x_arr_sz_container.get(); ++i)
        {
            y_arr[i] = base_taylor_raw_shape_project(x_arr[i], coeff_arr, coeff_arr_sz_container, stdx::Tag<PromotedFloatType>{}, bound_check);
        }
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

        PromotedFloatType carry_multiplier = 1u;

        for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
        {
            euclid_coeff_arr[i] = carry_multiplier * std::sin(static_cast<PromotedFloatType>(radian_coeff_arr[i]));
            carry_multiplier    *= std::cos(static_cast<PromotedFloatType>(radian_coeff_arr[i]));
        }
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    constexpr auto base_taylor_shape_project(FloatType x,
                                             const FloatType * radian_coeff_arr, SzContainer coeff_arr_sz_container,
                                             const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                                             const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        if constexpr(HasBoundCheck)
        {
            if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT) [[unlikely]]
            {
                throw std::invalid_argument("bad coefficient base size, max reached");
            }
        }
        else
        {
            if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT)
            {
                std::unreachable();
            }
        }

        // FloatType euclidean_coeff_space[MAX_BASE_COEFFICIENT];
        // taylor_radian_to_euclidean_space(radian_coeff_arr, coeff_arr_sz_container, euclidean_coeff_space, promotion_tag);

        return base_taylor_raw_shape_project(x,
                                             radian_coeff_arr, coeff_arr_sz_container,
                                             promotion_tag,
                                             bound_check);
    }

    template <class FloatType, class BatchSizeContainer, class SzContainer, class PromotedFloatType, bool HasBoundCheck = false>
    constexpr void base_batch_taylor_shape_project(const FloatType * x_arr, BatchSizeContainer x_arr_sz_container,
                                                   const FloatType * radian_coeff_arr, SzContainer coeff_arr_sz_container,
                                                   PromotedFloatType * y_arr,
                                                   const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        if constexpr(HasBoundCheck)
        {
            if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT) [[unlikely]]
            {
                throw std::invalid_argument("bad coefficient base size, max reached");
            }
        }
        else
        {
            if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT)
            {
                std::unreachable();
            }
        }

        // FloatType euclidean_coeff_space[MAX_BASE_COEFFICIENT];
        // taylor_radian_to_euclidean_space(radian_coeff_arr, coeff_arr_sz_container, euclidean_coeff_space, stdx::Tag<PromotedFloatType>{});

        base_batch_taylor_raw_shape_project(x_arr, x_arr_sz_container,
                                            radian_coeff_arr, coeff_arr_sz_container,
                                            y_arr,
                                            bound_check);
    }

    template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
    constexpr auto taylor_shape_project(FloatType x,
                                        const FloatType * radian_coeff_arr, SzContainer coeff_arr_sz_container,
                                        const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        base_taylor_shape_project(x,
                                  radian_coeff_arr, coeff_arr_sz_container,
                                  promotion_tag,
                                  std::integral_constant<bool, true>{});
    }

    //
    constexpr auto get_multivariate_taylor_shape_projection_coefficient_size(size_t in_feature_sz, size_t base_coeff_sz) -> size_t
    {
        if (in_feature_sz == 0u)
        {
            throw std::invalid_argument("bad in feature size, 0");
        }

        if (base_coeff_sz > MAX_BASE_COEFFICIENT)
        {
            throw std::invalid_argument("bad base coefficient size, max reached");
        }

        return std::pow(base_coeff_sz, in_feature_sz);
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType>
    constexpr void check_base_multivariate_taylor_shape_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
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
        size_t required_sz  = get_multivariate_taylor_shape_projection_coefficient_size(x_arr_sz_container.get(), base_coeff_sz_container.get());

        if (rem_coeff_sz < required_sz)
        {
            throw std::invalid_argument("insufficient remaining coefficient size");
        }
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
    constexpr auto base_multivariate_taylor_shape_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
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

            return base_taylor_shape_project(x_arr[0], coeff_arr_arg, base_coeff_sz_container, promotion_tag, bound_check);
        }

        PromotedFloatType projected_result  = 0;
        PromotedFloatType x_multiplier      = fast_approx_one_half(x_arr[0]);

        for (size_t i = 0u; i < base_coeff_sz_container.get(); ++i)
        {
            PromotedFloatType coeff         = base_multivariate_taylor_shape_project(std::next(x_arr), stdx::safe_minus_one(x_arr_sz_container),
                                                                                     base_coeff_sz_container,
                                                                                     coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                     promotion_tag,
                                                                                     bound_check);

            if (i == 0u)
            {
                projected_result    += coeff;
            }
            else
            {
                projected_result    += coeff * x_multiplier;
                x_multiplier        = fast_approx_one_half(x_multiplier);
            }
        }

        return projected_result;
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr auto multivariate_taylor_shape_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                     CoeffSizeContainer base_coeff_sz_container,
                                                     const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                     const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        check_base_multivariate_taylor_shape_project(x_arr, x_arr_sz_container,
                                                     base_coeff_sz_container,
                                                     coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                     promotion_tag);

        return base_multivariate_taylor_shape_project(x_arr, x_arr_sz_container,
                                                      base_coeff_sz_container,
                                                      coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                      promotion_tag,
                                                      std::integral_constant<bool, false>{});
    }

    constexpr auto get_multidimensional_taylor_shape_projection_coefficient_size(size_t in_feature_sz,
                                                                                 size_t base_coeff_sz,
                                                                                 size_t out_feature_sz,
                                                                                 bool has_logit_reuse) -> size_t
    {
        return get_multivariate_taylor_shape_projection_coefficient_size(in_feature_sz, base_coeff_sz) * out_feature_sz;
    }

    template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr __attribute__((noinline)) void multidimensional_taylor_shape_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
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

            output_arr[i]   = multivariate_taylor_shape_project(x_arr, x_arr_sz_container,
                                                                base_coeff_sz_container,
                                                                coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                promotion_tag);
        }
    }

    //

    constexpr auto get_batch_multivariate_taylor_shape_projection_coefficient_size(size_t in_feature_sz,
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
    constexpr void check_base_batch_multivariate_taylor_shape_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                      CoeffSizeContainer base_coeff_sz_container,
                                                                      const FloatType * radian_coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
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
        size_t required_sz  = get_batch_multivariate_taylor_shape_projection_coefficient_size(x_arr_sz_container.get(), base_coeff_sz_container.get(), batch_sz_container.get());

        if (rem_coeff_sz < required_sz)
        {
            throw std::invalid_argument("insufficient remaining coefficient size");
        }
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType, bool HasBoundCheck = true>
    constexpr void base_batch_multivariate_taylor_shape_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                CoeffSizeContainer base_coeff_sz_container,
                                                                const FloatType * radian_coeff_arr, size_t& radian_coeff_arr_offset, size_t radian_coeff_arr_cap,
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
            const size_t tentative_nxt_offset   = radian_coeff_arr_offset + base_coeff_sz_container.get();

            if constexpr(HasBoundCheck)
            {
                if (tentative_nxt_offset > radian_coeff_arr_cap) [[unlikely]]
                {
                    throw std::runtime_error("coefficient vector ran out of space");
                }
            }

            const FloatType * radian_coeff_arr_arg  = std::next(radian_coeff_arr, radian_coeff_arr_offset);
            radian_coeff_arr_offset                 = tentative_nxt_offset;

            base_batch_taylor_shape_project(flat_x_arr_arr, batch_sz_container,
                                            radian_coeff_arr_arg, base_coeff_sz_container,
                                            y_arr,
                                            bound_check);

            return;
        }

        alignas(alignof(std::max_align_t)) PromotedFloatType projected_arr[batch_sz_container.get()];
        alignas(alignof(std::max_align_t)) PromotedFloatType x_multiplier_arr[batch_sz_container.get()];

        for (size_t i = 0u; i < batch_sz_container.get(); ++i)
        {
            x_multiplier_arr[i]   = fast_approx_one_half(flat_x_arr_arr[i]);
        }

        std::fill(y_arr, std::next(y_arr, batch_sz_container.get()), 0);

        for (size_t i = 0u; i < base_coeff_sz_container.get(); ++i)
        {
            base_batch_multivariate_taylor_shape_project(std::next(flat_x_arr_arr, batch_sz_container.get()), stdx::safe_minus_one(x_arr_sz_container), batch_sz_container,
                                                         base_coeff_sz_container,
                                                         radian_coeff_arr, radian_coeff_arr_offset, radian_coeff_arr_cap,
                                                         projected_arr,
                                                         bound_check);

            if (i == 0)
            {
                for (size_t j = 0u; j < batch_sz_container.get(); ++j)
                {
                    y_arr[j]   += projected_arr[j];
                }
            }
            else
            {
                for (size_t j = 0u; j < batch_sz_container.get(); ++j)
                {
                    y_arr[j]            += projected_arr[j] * x_multiplier_arr[j];
                    x_multiplier_arr[j] = fast_approx_one_half(x_multiplier_arr[j]);
                }
            }
        }
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType>
    constexpr __attribute__((noinline)) void batch_multivariate_taylor_shape_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                     CoeffSizeContainer base_coeff_sz_container,
                                                                                     const FloatType * radian_coeff_arr, size_t& radian_coeff_arr_offset, size_t radian_coeff_arr_cap,
                                                                                     PromotedFloatType * y_arr)
    {
        check_base_batch_multivariate_taylor_shape_project(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                                           base_coeff_sz_container,
                                                           radian_coeff_arr, radian_coeff_arr_offset, radian_coeff_arr_cap,
                                                           y_arr);

        base_batch_multivariate_taylor_shape_project(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                                     base_coeff_sz_container,
                                                     radian_coeff_arr, radian_coeff_arr_offset, radian_coeff_arr_cap,
                                                     y_arr,
                                                     std::integral_constant<bool, false>{});
    }

    constexpr auto get_batch_multidimensional_taylor_shape_projection_coefficient_size(size_t in_feature_sz,
                                                                                       size_t batch_sz,
                                                                                       size_t base_coeff_sz,
                                                                                       size_t out_feature_sz,
                                                                                       bool has_logit_reuse) -> size_t
    {
        return get_batch_multivariate_taylor_shape_projection_coefficient_size(in_feature_sz, base_coeff_sz, batch_sz) * out_feature_sz;
    }

    template <class FloatType, class XArrSizeContainer, class BatchSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
    constexpr __attribute__((noinline)) void batch_multidimensional_taylor_shape_project(const FloatType * flat_x_arr_arr, XArrSizeContainer x_arr_sz_container, BatchSizeContainer batch_sz_container,
                                                                                         CoeffSizeContainer base_coeff_sz_container,
                                                                                         const FloatType * radian_coeff_arr, size_t& radian_coeff_arr_offset, size_t radian_coeff_arr_cap,
                                                                                         PromotedFloatType * flat_output_arr_arr, size_t output_dimension_sz,
                                                                                         bool has_logit_reuse_tag = true)
    {
        PromotedFloatType * it_flat_output_arr_arr = flat_output_arr_arr;

        for (size_t i = 0u; i < output_dimension_sz; ++i)
        {
            batch_multivariate_taylor_shape_project(flat_x_arr_arr, x_arr_sz_container, batch_sz_container,
                                                    base_coeff_sz_container,
                                                    radian_coeff_arr, radian_coeff_arr_offset, radian_coeff_arr_cap,
                                                    it_flat_output_arr_arr);

            std::advance(it_flat_output_arr_arr, batch_sz_container.get());
        }
    }
}

#endif