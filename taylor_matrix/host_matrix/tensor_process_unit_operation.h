//HEADER_CONTROL 3

#ifndef __TAYLOR_MATRIX_HOST_MATRIX_TENSOR_PROCESS_UNIT_OPERATION_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_TENSOR_PROCESS_UNIT_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include <stl_extension/stdx.h>
#include <vector>
#include <matrix/tensor_model.h>
#include <stdexcept>
#include "taylor_projection.h"
#include "shape_projection.h"
#include <array>
#include "generic_one_dimensional_cubic_interpolation.h"
#include "generic_two_dimensional_cubic_interpolation.h"
#include "local_exception.h"

namespace taylor_matrix::host_matrix::tensor_process_unit_operation
{
    static inline constexpr size_t PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ = tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;

    template <class ...Args>
    constexpr auto make_process_unit_from_shape_vec(const std::vector<size_t, Args...>& space) -> tensor_model::ProcessUnit
    {
        if (space.size() != 1u)
        {
            throw std::invalid_argument("bad space shape, incompatible");
        }

        if (space.front() != PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ)
        {
            throw std::invalid_argument("bad space shape, incompatible");
        }

        return {.logit_vec = stdx::to_array_default_initializer(0)};
    }

    constexpr void internal_unflatten(tensor_model::ProcessUnit& arg,
                                      const tensor_model::tensor_std_float_t * input_arr, size_t input_arr_sz,
                                      size_t& offset)
    {
        for (auto& logit: arg.logit_vec)
        {
            logit = input_arr[stdx::access_guard(offset++, input_arr_sz)];
        }
    }

    template <class ...Args, class ...Args1>
    constexpr auto make_process_unit_from_flat_vec(const std::vector<size_t, Args...>& space,
                                                   const std::vector<tensor_model::tensor_std_float_t, Args1...>& input_vec) -> tensor_model::ProcessUnit
    {
        tensor_model::ProcessUnit rs                        = make_process_unit_from_shape_vec(space);

        const tensor_model::tensor_std_float_t * input_arr  = input_vec.data();
        size_t input_arr_sz                                 = input_vec.size();
        size_t input_arr_offset                             = 0u;

        internal_unflatten(rs, input_arr, input_arr_sz, input_arr_offset);

        return rs;
    }

    constexpr auto empty_as(const tensor_model::tensor_std_float_t&) -> tensor_model::tensor_std_float_t
    {
        return 0;
    }

    constexpr auto empty_as(const tensor_model::ProcessUnit&) -> tensor_model::ProcessUnit
    {
        return {.logit_vec = stdx::to_array_default_initializer(0)};
    }

    constexpr auto accumulate(const tensor_model::ProcessUnit& lhs, const tensor_model::ProcessUnit& rhs) -> tensor_model::ProcessUnit
    {
        tensor_model::ProcessUnit rs{};

        for (size_t i = 0u; i < lhs.logit_vec.size(); ++i)
        {
            rs.logit_vec[i] = lhs.logit_vec[i] + rhs.logit_vec[i];
        }

        return rs;
    }

    constexpr auto accumulate(const tensor_model::ProcessUnit * process_vec, size_t process_vec_sz) -> tensor_model::ProcessUnit
    {
        if (process_vec_sz == 0u) [[unlikely]]
        {
            throw std::invalid_argument("bad accumulation size, 0");
        }

        tensor_model::ProcessUnit result = process_vec[0];

        for (size_t i = 1u; i < process_vec_sz; ++i)
        {
            result = accumulate(result, process_vec[i]);
        }

        return result;
    }

    template <class ValueType>
    constexpr auto div(const tensor_model::ProcessUnit& process_unit, const ValueType& value) -> tensor_model::ProcessUnit
    {
        tensor_model::ProcessUnit rs{};

        for (size_t i = 0u; i < rs.logit_vec.size(); ++i)
        {
            rs.logit_vec[i] = process_unit.logit_vec[i] / value;
        }

        return rs;
    }

    constexpr auto avg(const tensor_model::ProcessUnit * process_vec, size_t process_vec_sz) -> tensor_model::ProcessUnit
    {
        return div(accumulate(process_vec, process_vec_sz), stdx::safe_non_zero_access(process_vec_sz));
    }

    template <class TaylorBaseCoeffSizeContainer,
              class TaylorBasePromotedFloatType = tensor_model::tensor_std_float_t>
    constexpr __attribute__((noinline)) auto intercourse_process_unit(const tensor_model::ProcessUnit& lhs,
                                                                      const tensor_model::ProcessUnit& rhs,
                                                                      TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                                      const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                      const stdx::Tag<TaylorBasePromotedFloatType>& promotion_tag = stdx::Tag<TaylorBasePromotedFloatType>{},
                                                                      bool has_logit_reuse_tag = true) -> tensor_model::ProcessUnit
    {
        constexpr size_t COMBINED_DIMENSION_SZ = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * 2u;

        std::array<tensor_model::tensor_std_float_t, COMBINED_DIMENSION_SZ> combined{};

        std::copy(lhs.logit_vec.begin(), lhs.logit_vec.end(), combined.data());
        std::copy(rhs.logit_vec.begin(), rhs.logit_vec.end(), std::next(combined.data(), lhs.logit_vec.size()));

        std::array<tensor_model::tensor_std_float_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ> child{};

        shape_projection::multidimensional_taylor_shape_project(combined.data(), stdx::to_size_container(std::integral_constant<size_t, COMBINED_DIMENSION_SZ>{}),
                                                                base_coeff_sz_container,
                                                                coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                child.data(), child.size(),
                                                                promotion_tag,
                                                                has_logit_reuse_tag);

        return {.logit_vec = child};
    }

    //I don't like that, so let's settle on another topic for now

    //we'd start the engineering of virtual addressing

    //I rather think that this is still key-value problems, yeah, we need to use Redis, or we don't?
    //because TCP does not provide such low-latency and solve the unit problem like we do, we don't know if we can rely on the protocol to massively transfer an insane number of data to the observer pool

    //what I'd imagine is that we have replicas, binary-tree replicas, gossiping to pass down the values, make it a high availability cluster of key-value pairs
    //apart from replica, we'd have partition factors, to partition the key-value pairs into appropriate storage engines

    //we'd build security protocol

    //and we'd build virtual addressing on top of the key-value engine

    //that's a lot of work, but we'd try to be concise about filesystem, RAM + etc.

    template <class QuantizationMachine1D,
              class QuantizationMachine2D,
              class PromotedFloatType = tensor_model::tensor_std_float_t>
    constexpr __attribute__((noinline)) auto interpolate_process_unit(const tensor_model::ProcessUnit& lhs,
                                                                      const tensor_model::ProcessUnit& rhs,
                                                                      QuantizationMachine1D&& quant_machine_1d,
                                                                      QuantizationMachine2D&& quant_machine_2d,
                                                                      const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                      const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                                                                      bool has_logit_reuse_tag = true) -> tensor_model::ProcessUnit
    {
        using namespace taylor_matrix::host_matrix::generic_two_dimensional_cubic_interpolation;

        (void) quant_machine_1d;

        std::array<tensor_model::tensor_std_float_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ> child{};

        const size_t transform_count    = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;
        const size_t required_sz        = transform_count * get_cubic_interpolated_2d_projection_size(quant_machine_2d);
        const size_t post_offset        = coeff_arr_offset + required_sz;

        if (post_offset > coeff_arr_cap)
        {
            throw local_exception::insufficient_logit_vec_size();
        }

        for (size_t i = 0u; i < PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ; ++i)
        {
            for (size_t j = 0u; j < PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ; ++j)
            {
                child[i]    += cubic_interpolated_2d_project(lhs.logit_vec[i],
                                                             rhs.logit_vec[j],
                                                             quant_machine_2d,
                                                             coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                             promotion_tag,
                                                             std::integral_constant<bool, false>{});
            }

            child[i] /= PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;
        }

        return {.logit_vec = child};
    }

    template <class TaylorBaseCoeffSizeContainer,
              size_t BATCH_SZ,
              class TaylorBasePromotedFloatType = tensor_model::tensor_std_float_t>
    constexpr __attribute__((noinline)) void batch_intercourse_process_unit(const tensor_model::ProcessUnit * lhs_arr,
                                                                            const tensor_model::ProcessUnit * rhs_arr,
                                                                            const std::integral_constant<size_t, BATCH_SZ> batch_sz,
                                                                            tensor_model::ProcessUnit * out_arr,
                                                                            TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                                            const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                            const stdx::Tag<TaylorBasePromotedFloatType>& promotion_tag = stdx::Tag<TaylorBasePromotedFloatType>{},
                                                                            bool has_logit_reuse_tag = true)
    {
        static_assert(BATCH_SZ != 0u);

        constexpr size_t COMBINED_DIMENSION_SZ  = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * 2u;
        constexpr size_t INPUT_ARRAY_SZ         = COMBINED_DIMENSION_SZ * BATCH_SZ;
        constexpr size_t OUTPUT_ARRAY_SZ        = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * BATCH_SZ;

        tensor_model::tensor_std_float_t input_arr[INPUT_ARRAY_SZ];
        TaylorBasePromotedFloatType taylor_output_arr[OUTPUT_ARRAY_SZ];

        for (size_t i = 0u; i < BATCH_SZ; ++i)
        {
            for (size_t j = 0u; j < PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ; ++j)
            {
                size_t col_idx  = i;
                size_t row_idx  = j;
                size_t idx      = row_idx * BATCH_SZ + col_idx;

                input_arr[idx]  = lhs_arr[i].logit_vec[j];
            }

            for (size_t j = 0u; j < PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ; ++j)
            {
                size_t col_idx  = i;
                size_t row_idx  = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ + j;
                size_t idx      = row_idx * BATCH_SZ + col_idx;

                input_arr[idx]  = rhs_arr[i].logit_vec[j];
            }
        }

        shape_projection::batch_multidimensional_taylor_shape_project(input_arr, stdx::to_size_container(std::integral_constant<size_t, COMBINED_DIMENSION_SZ>{}), stdx::to_size_container(batch_sz),
                                                                      base_coeff_sz_container,
                                                                      coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                      taylor_output_arr, std::integral_constant<size_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ>{},
                                                                      has_logit_reuse_tag);

        for (size_t i = 0u; i < BATCH_SZ; ++i)
        {
            out_arr[i] = tensor_model::ProcessUnit{};

            for (size_t j = 0u; j < PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ; ++j)
            {
                size_t col_idx  = i;
                size_t row_idx  = j;
                size_t idx      = row_idx * BATCH_SZ + col_idx;

                out_arr[i].logit_vec[j] = taylor_output_arr[idx];   
            }
        }
    }

    template <class QuantizationMachine1D,
              class QuantizationMachine2D,
              size_t BATCH_SZ,
              class PromotedFloatType = tensor_model::tensor_std_float_t>
    constexpr __attribute__((noinline)) void batch_interpolate_process_unit(const tensor_model::ProcessUnit * lhs_arr,
                                                                            const tensor_model::ProcessUnit * rhs_arr,
                                                                            const std::integral_constant<size_t, BATCH_SZ> batch_sz,
                                                                            tensor_model::ProcessUnit * out_arr,
                                                                            QuantizationMachine1D&& quant_machine_1d,
                                                                            QuantizationMachine2D&& quant_machine_2d,
                                                                            const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                            const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                                                                            bool has_logit_reuse_tag = true)
    {
        const size_t saved_coeff_arr_offset = coeff_arr_offset;

        for (size_t i = 0u; i < BATCH_SZ; ++i)
        {
            coeff_arr_offset = saved_coeff_arr_offset;

            out_arr[i] = interpolate_process_unit(lhs_arr[i],
                                                  rhs_arr[i],
                                                  quant_machine_1d,
                                                  quant_machine_2d,
                                                  coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                  promotion_tag,
                                                  has_logit_reuse_tag);
        }
    }

    template <class QuantizationMachine1D,
              class QuantizationMachine2D,
              class PromotedFloatType = tensor_model::tensor_std_float_t>
    constexpr auto mono_transform(const tensor_model::ProcessUnit& arg,
                                  QuantizationMachine1D&& quant_machine_1d,
                                  QuantizationMachine2D&& quant_machine_2d,
                                  const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                  const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>()) -> tensor_model::ProcessUnit
    {
        using namespace taylor_matrix::host_matrix::generic_one_dimensional_cubic_interpolation;

        tensor_model::ProcessUnit rs0{};

        {
            tensor_model::ProcessUnit xx = interpolate_process_unit(arg,
                                                                    arg,
                                                                    quant_machine_1d,
                                                                    quant_machine_2d,
                                                                    coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                    promotion_tag,
                                                                    false);

            tensor_model::ProcessUnit avg_arr[]{arg, xx};
            rs0 = avg(avg_arr, 2u);
        }

        tensor_model::ProcessUnit rs1{};

        {
            tensor_model::ProcessUnit sub_xx{};

            const size_t transform_count    = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;
            const size_t post_offset        = coeff_arr_offset + transform_count * get_cubic_interpolated_projection_size(quant_machine_1d);

            if (post_offset > coeff_arr_cap)
            {
                throw local_exception::insufficient_logit_vec_size();
            }

            for (size_t i = 0u; i < PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ; ++i)
            {
                sub_xx.logit_vec[i] = cubic_interpolated_project(rs0.logit_vec[i],
                                                                 quant_machine_1d,
                                                                 coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                 promotion_tag,
                                                                 std::integral_constant<bool, false>{});
            }

            tensor_model::ProcessUnit avg_arr[]{rs0, sub_xx};
            rs1 = avg(avg_arr, 2u);
        }

        return rs1;
    }

    constexpr auto deparameterize(const tensor_model::ProcessUnit& process_unit, double perc) -> tensor_model::ProcessUnit
    {
        return {.logit_vec = stdx::copy_and_trail_defaultize(process_unit.logit_vec,
                                                             perc,
                                                             static_cast<tensor_model::tensor_std_float_t (*)(const tensor_model::tensor_std_float_t&)>(empty_as))};
    }

    template <class FloatType>
    constexpr auto positional_encode(const tensor_model::ProcessUnit& arg,
                                     const FloatType& amplitude,
                                     const FloatType& frequency_multiplier,
                                     const FloatType& x_offset,
                                     const FloatType& y_offset,
                                     size_t offset) -> tensor_model::ProcessUnit
    {
        if (offset >= arg.logit_vec.size())
        {
            throw std::invalid_argument("bad offset, out of bound access");
        }

        tensor_model::ProcessUnit rs    = arg;
        rs.logit_vec[offset]            += amplitude * std::sin(frequency_multiplier * arg.logit_vec[offset] + x_offset) + y_offset;

        return rs;
    }

    template <class ...Args>
    constexpr void flatten(const tensor_model::ProcessUnit& arg, std::vector<tensor_model::tensor_std_float_t, Args...>& output_vec)
    {
        for (const auto& logit: arg.logit_vec)
        {
            output_vec.push_back(logit);
        }
    }

    template <class ...Args>
    constexpr void get_shape(const tensor_model::ProcessUnit& arg,
                             std::vector<size_t, Args...>& output_vec)
    {
        output_vec.push_back(tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ);
    }
}

#endif