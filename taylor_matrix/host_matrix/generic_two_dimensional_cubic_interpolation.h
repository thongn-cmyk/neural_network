#ifndef __TAYLOR_MATRIX_HOST_MATRIX_GENERIC_TWO_DIMENSIONAL_CUBIC_INTERPOLATION_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_GENERIC_TWO_DIMENSIONAL_CUBIC_INTERPOLATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "local_exception.h"

namespace taylor_matrix::host_matrix::generic_two_dimensional_cubic_interpolation
{
    consteval auto get_cubic_interpolated_2d_projection_base_size() -> size_t
    {
        return 3u;
    }

    template <class QuantizationMachine>
    constexpr auto get_cubic_interpolated_2d_projection_size(QuantizationMachine&& quant_machine)
    {
        const size_t ARRAY_SZ       = quant_machine.quantization_size() * quant_machine.quantization_size();
        constexpr size_t BASE_SZ    = get_cubic_interpolated_2d_projection_base_size();

        return ARRAY_SZ * BASE_SZ;
    }

    template <class FloatType,
              class QuantizationMachine,
              class PromotedFloatType = FloatType,
              bool HasBoundCheck = true>
    constexpr auto cubic_interpolated_2d_project(FloatType x0, FloatType x1,
                                                 QuantizationMachine&& quant_machine,
                                                 const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                 const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>(),
                                                 const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
    {
        const size_t REQUIRED_SZ    = get_cubic_interpolated_2d_projection_size(quant_machine);
        size_t next_offset          = coeff_arr_offset + REQUIRED_SZ;

        if constexpr(HasBoundCheck)
        {
            if (next_offset > coeff_arr_cap)
            {
                throw local_exception::insufficient_logit_vec_size();
            }
        }

        size_t quant_d_sz           = quant_machine.quantization_size();
        intmax_t x0_quant_slot      = quant_machine.quantitize(x0);
        intmax_t x1_quant_slot      = quant_machine.quantitize(x1);
        intmax_t flat_slot          = x0_quant_slot * quant_d_sz + x1_quant_slot;

        size_t offset_0_1           = flat_slot * get_cubic_interpolated_2d_projection_base_size();
        size_t global_offset_0_1    = coeff_arr_offset + offset_0_1;

        PromotedFloatType a         = coeff_arr[global_offset_0_1];
        PromotedFloatType b         = coeff_arr[global_offset_0_1 + 1];
        PromotedFloatType c         = coeff_arr[global_offset_0_1 + 2];
        PromotedFloatType y_0_1     = a * x0 + b * x1 + c;

        coeff_arr_offset            = next_offset;

        return y_0_1;
    }
}

#endif