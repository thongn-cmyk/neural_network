#ifndef __TAYLOR_MATRIX_HOST_MATRIX_GENERIC_ONE_DIMENSIONAL_CUBIC_INTERPOLATION_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_GENERIC_ONE_DIMENSIONAL_CUBIC_INTERPOLATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "local_exception.h"

namespace taylor_matrix::host_matrix::generic_one_dimensional_cubic_interpolation
{
    consteval auto get_cubic_interpolated_projection_base_size() -> size_t
    {
        return 2u;
    }

    template <class QuantizationMachine>
    constexpr auto get_cubic_interpolated_projection_size(QuantizationMachine&& quant_machine) -> size_t
    {
        return quant_machine.quantization_size() * get_cubic_interpolated_projection_base_size();
    }

    template <class FloatType,
              class QuantizationMachine,
              class PromotedFloatType = FloatType,
              bool HasBoundCheck = true>
    constexpr auto cubic_interpolated_project(FloatType x,
                                              QuantizationMachine&& quant_machine,
                                              const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                              const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                                              const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{})
    {
        PromotedFloatType result_y0     = 0;

        size_t required_sz              = get_cubic_interpolated_projection_size(quant_machine);
        size_t next_offset              = coeff_arr_offset + required_sz;

        if constexpr(HasBoundCheck)
        {
            if (next_offset > coeff_arr_cap)
            {
                throw local_exception::insufficient_logit_vec_size();
            }
        }

        intmax_t quant_slot             = quant_machine.quantitize(x);
        size_t y_offset                 = coeff_arr_offset + quant_slot * get_cubic_interpolated_projection_base_size();

        PromotedFloatType a             = coeff_arr[y_offset + 0];
        PromotedFloatType b             = coeff_arr[y_offset + 1];

        PromotedFloatType cur_y         = a * x + b;
        coeff_arr_offset                = next_offset;

        return cur_y;
    }
}

#endif