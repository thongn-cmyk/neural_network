#ifndef __TAYLOR_MATRIX_HOST_MATRIX_ONE_DIMENSIONAL_CUBIC_INTERPOLATION_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_ONE_DIMENSIONAL_CUBIC_INTERPOLATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "taylor_projection.h"
#include "cubic_quantization_machine.h"

namespace taylor_matrix::host_matrix::one_dimensional_cubic_interpolation
{
    consteval auto get_cubic_interpolated_projection_base_size() -> size_t
    {
        return 2u;
    }

    constexpr auto get_cubic_exp_interpolated_projection_size() -> size_t
    {
        using ExponentialQuantizationMachine    = taylor_matrix::host_matrix::cubic_quantization_machine::StandardCubicInterpolationExponentialQuantizationMachine;

        return get_cubic_interpolated_projection_base_size() * ExponentialQuantizationMachine{}.quantization_size();
    }

    template <class FloatType,
              class PromotedFloatType = FloatType,
              bool HasBoundCheck = true>
    constexpr auto cubic_exp_interpolated_project(FloatType x,
                                                  const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                  const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                                                  const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
    {
        using ExponentialQuantizationMachine    = taylor_matrix::host_matrix::cubic_quantization_machine::StandardCubicInterpolationExponentialQuantizationMachine;

        using namespace taylor_matrix::host_matrix::taylor_projection;

        constexpr double ALPHA          = 20;
        constexpr size_t BASE_SZ        = get_cubic_interpolated_projection_base_size();
        PromotedFloatType result_y0     = 0;

        ExponentialQuantizationMachine exp_quant_machine{};

        size_t required_sz          = BASE_SZ * exp_quant_machine.quantization_size();
        size_t next_offset          = coeff_arr_offset + required_sz;

        if constexpr(HasBoundCheck)
        {
            if (next_offset > coeff_arr_cap)
            {
                throw std::invalid_argument("insufficient remaning coefficient size");
            }
        }

        intmax_t quant_slot         = exp_quant_machine.quantitize(x);
        intmax_t prev_quant_slot    = std::max(intmax_t{0}, quant_slot - 1);

        // PromotedFloatType hinge_x   = exp_quant_machine.template region_first<PromotedFloatType>(quant_slot);

        // PromotedFloatType prev_y    = base_taylor_project(x,
        //                                                   std::next(coeff_arr, BASE_SZ * prev_quant_slot), stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>{}),
        //                                                   promotion_tag,
        //                                                   bound_check);

        PromotedFloatType cur_y     = base_taylor_project(x,
                                                          std::next(coeff_arr, BASE_SZ * quant_slot), stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>{}),
                                                          promotion_tag,
                                                          bound_check);

        // PromotedFloatType delta_x   = x - hinge_x;
        // PromotedFloatType prev_perc = 1 / std::scalbn(static_cast<PromotedFloatType>(1), ALPHA * delta_x);

        // result_y0                   = prev_perc * prev_y + (1 - prev_perc) * cur_y;
        coeff_arr_offset            = next_offset;

        // return result_y0;

        return cur_y;
    }
}

#endif