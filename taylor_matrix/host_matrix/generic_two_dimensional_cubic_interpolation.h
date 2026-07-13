#ifndef __TAYLOR_MATRIX_HOST_MATRIX_GENERIC_TWO_DIMENSIONAL_CUBIC_INTERPOLATION_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_GENERIC_TWO_DIMENSIONAL_CUBIC_INTERPOLATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "shape_projection.h"

namespace taylor_matrix::host_matrix::generic_two_dimensional_cubic_interpolation
{
    consteval auto get_cubic_exp_interpolated_2d_projection_base_size() -> size_t
    {
        return 3u;
    }

    // template <class QuantizationMachine>
    // constexpr auto get_cubic_exp_interpolated_2d_projection_size(QuantizationMachine&& quant_machine)
    // {
    //     constexpr size_t BASE_SZ    = get_cubic_exp_interpolated_2d_projection_base_size();
    //     const size_t ARRAY_SZ       = quant_machine.quantization_size() * quant_machine.quantization_size();

    //     return BASE_SZ * ARRAY_SZ;
    // }

    // template <class FloatType,
    //           class QuantizationMachine,
    //           class PromotedFloatType = FloatType,
    //           bool HasBoundCheck = true>
    // constexpr auto cubic_exp_interpolated_2d_project(FloatType x0, FloatType x1,
    //                                                  QuantizationMachine&& quant_machine,
    //                                                  const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
    //                                                  const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>(),
    //                                                  const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
    // {
    //     using ExponentialQuantizationMachine    = taylor_matrix::host_matrix::cubic_quantization_machine::StandardCubicInterpolationExponentialQuantizationMachine;

    //     using namespace taylor_matrix::host_matrix::shape_projection;

    //     constexpr double ALPHA      = 20u;
    //     constexpr size_t BASE_SZ    = get_cubic_exp_interpolated_2d_projection_base_size();

    //     const size_t REQUIRED_SZ    = get_cubic_exp_interpolated_2d_projection_size(quant_machine);
    //     size_t next_offset          = coeff_arr_offset + REQUIRED_SZ;

    //     if constexpr(HasBoundCheck)
    //     {
    //         if (next_offset > coeff_arr_cap)
    //         {
    //             throw std::invalid_argument("insufficient remaining coefficient size");
    //         }
    //     }

    //     size_t quant_d_sz           = quant_machine.quantization_size();

    //     intmax_t x0_quant_slot      = quant_machine.quantitize(x0);
    //     // intmax_t x0_prev_quant_slot = std::max(intmax_t{0}, x0_quant_slot - 1);

    //     intmax_t x1_quant_slot      = quant_machine.quantitize(x1);
    //     // intmax_t x1_prev_quant_slot = std::max(intmax_t{0}, x1_quant_slot - 1);

    //     FloatType x_arr[]{x0, x1};

    //     // PromotedFloatType hinge_x0  = quant_machine.template region_first<PromotedFloatType>(x0_quant_slot);
    //     // PromotedFloatType hinge_x1  = quant_machine.template region_first<PromotedFloatType>(x1_quant_slot);

    //     size_t offset_0_1           = x0_quant_slot * quant_d_sz + x1_quant_slot;
    //     size_t global_offset_0_1    = coeff_arr_offset + offset_0_1;
    //     PromotedFloatType y_0_1     = multivariate_taylor_shape_project(x_arr, stdx::to_size_container(std::integral_constant<size_t, 2>{}),
    //                                                                     stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>{}),
    //                                                                     coeff_arr, global_offset_0_1, coeff_arr_cap,
    //                                                                     promotion_tag);

    //     // size_t offset_0_1p          = x0_quant_slot * quant_d_sz + x1_prev_quant_slot;
    //     // size_t global_offset_0_1p   = coeff_arr_offset + offset_0_1p;
    //     // PromotedFloatType y_0_1p    = multivariate_taylor_shape_project(x_arr, stdx::to_size_container(std::integral_constant<size_t, 2>{}),
    //     //                                                                 stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>{}),
    //     //                                                                 coeff_arr, global_offset_0_1p, coeff_arr_cap,
    //     //                                                                 promotion_tag);

    //     // size_t offset_0p_1          = x0_prev_quant_slot * quant_d_sz + x1_quant_slot;
    //     // size_t global_offset_0p_1   = coeff_arr_offset + offset_0p_1;
    //     // PromotedFloatType y_0p_1    = multivariate_taylor_shape_project(x_arr, stdx::to_size_container(std::integral_constant<size_t, 2>{}),
    //     //                                                                 stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>{}),
    //     //                                                                 coeff_arr, global_offset_0p_1, coeff_arr_cap,
    //     //                                                                 promotion_tag);

    //     // size_t offset_0p_1p         = x0_prev_quant_slot * quant_d_sz + x1_prev_quant_slot;
    //     // size_t global_offset_0p_1p  = coeff_arr_offset + offset_0p_1p;
    //     // PromotedFloatType y_0p_1p   = multivariate_taylor_shape_project(x_arr, stdx::to_size_container(std::integral_constant<size_t, 2>{}),
    //     //                                                                 stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>{}),
    //     //                                                                 coeff_arr, global_offset_0p_1p, coeff_arr_cap,
    //     //                                                                 promotion_tag);

    //     // PromotedFloatType delta_x_0_1p  = x1 - hinge_x1;
    //     // PromotedFloatType x_0_1p_perc   = 1 / std::scalbn(static_cast<PromotedFloatType>(1), ALPHA * delta_x_0_1p);
    //     // PromotedFloatType y_0_1p_perc   = 1 - x_0_1p_perc;

    //     // PromotedFloatType delta_x_0p_1  = x0 - hinge_x0;
    //     // PromotedFloatType x_0p_1_perc   = 1 / std::scalbn(static_cast<PromotedFloatType>(1), ALPHA * delta_x_0p_1);
    //     // PromotedFloatType y_0p_1_perc   = 1 - x_0p_1_perc;

    //     // PromotedFloatType result        = y_0_1 * y_0_1p_perc * y_0p_1_perc
    //     //                                   + y_0_1p * x_0_1p_perc * y_0p_1_perc
    //     //                                   + y_0p_1 * x_0p_1_perc * y_0_1p_perc
    //     //                                   + y_0p_1p * x_0p_1_perc * x_0_1p_perc;

    //     coeff_arr_offset                = next_offset;

    //     // return result;

    //     return y_0_1;
    // }

    template <class QuantizationMachine>
    constexpr auto get_cubic_exp_interpolated_2d_projection_size(QuantizationMachine&& quant_machine)
    {
        constexpr size_t BASE_SZ    = get_cubic_exp_interpolated_2d_projection_base_size();

        return BASE_SZ;
    }

    template <class FloatType,
              class QuantizationMachine,
              class PromotedFloatType = FloatType,
              bool HasBoundCheck = true>
    constexpr auto cubic_exp_interpolated_2d_project(FloatType x0, FloatType x1,
                                                     QuantizationMachine&& quant_machine,
                                                     const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                     const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>(),
                                                     const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
    {
        using ExponentialQuantizationMachine    = taylor_matrix::host_matrix::cubic_quantization_machine::StandardCubicInterpolationExponentialQuantizationMachine;

        using namespace taylor_matrix::host_matrix::shape_projection;

        const size_t REQUIRED_SZ    = 3u;
        size_t next_offset          = coeff_arr_offset + REQUIRED_SZ;

        if constexpr(HasBoundCheck)
        {
            if (next_offset > coeff_arr_cap)
            {
                throw std::invalid_argument("insufficient remaining coefficient size");
            }
        }

        PromotedFloatType a         = coeff_arr[coeff_arr_offset];
        PromotedFloatType b         = coeff_arr[coeff_arr_offset + 1];
        PromotedFloatType c         = coeff_arr[coeff_arr_offset + 2];
        PromotedFloatType y_0_1     = a * x0 + b * x1 + c;
        coeff_arr_offset            = next_offset;

        return y_0_1;
    }
}

#endif