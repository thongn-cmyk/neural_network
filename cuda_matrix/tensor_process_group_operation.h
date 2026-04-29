#ifndef __CUDA_MATRIX_TENSOR_PROCESS_GROUP_OPERATION_H__
#define __CUDA_MATRIX_TENSOR_PROCESS_GROUP_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include "tensor_model.h"
#include <stdexcept>
#include "tensor_process_unit_operation.h"
#include <array>

namespace cuda_matrix::tensor_process_group_operation
{
    using namespace cuda_matrix::tensor_model;
    using namespace cuda_matrix::utility;

    template <class ShapeBaseCoeffSizeContainer,
              class ShapeBasePromotedFloatType = tensor_model::tensor_std_float_t,
              size_t BATCH_SZ = 64u>
    constexpr __device__ __attribute__((noinline)) auto two_to_one_project(const ProcessGroup& lhs,
                                                                           const ProcessGroup& rhs,
                                                                           ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                           const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                           const stdx::Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = stdx::Tag<ShapeBasePromotedFloatType>{},
                                                                           bool has_process_unit_logit_reuse_tag = true,
                                                                           bool has_process_group_logit_reuse_tag = true,
                                                                           const std::integral_constant<size_t, BATCH_SZ>& batch_sz = std::integral_constant<size_t, BATCH_SZ>{}) -> ProcessGroup
    {
        constexpr size_t TOTAL_ITERATION_SZ         = tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ * tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
        constexpr size_t REVOLUTION_SZ              = TOTAL_ITERATION_SZ / BATCH_SZ;
        constexpr size_t OFFSET_SZ                  = REVOLUTION_SZ * BATCH_SZ;
        constexpr size_t REM_SZ                     = TOTAL_ITERATION_SZ - OFFSET_SZ;

        static_assert(REM_SZ == 0u);

        const size_t saved_shape_coeff_arr_offset   = shape_coeff_arr_offset;

        ProcessGroup accum_vec[tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ];

        for (size_t i = 0u; i < REVOLUTION_SZ; ++i)
        {
            tensor_model::ProcessUnit lhs_tensor_arr[BATCH_SZ];
            tensor_model::ProcessUnit rhs_tensor_arr[BATCH_SZ];
            tensor_model::ProcessUnit out_tensor_arr[BATCH_SZ];

            for (size_t j = 0u; j < BATCH_SZ; ++j)
            {
                const size_t virtual_idx    = i * BATCH_SZ + j;
                const size_t actual_i       = virtual_idx / tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
                const size_t actual_j       = virtual_idx % tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;

                lhs_tensor_arr[j]           = lhs.process_vec[actual_i];
                rhs_tensor_arr[j]           = rhs.process_vec[actual_j];
            }

            shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;

            tensor_process_unit_operation::batch_two_to_one_project(lhs_tensor_arr,
                                                                    rhs_tensor_arr,
                                                                    batch_sz,
                                                                    out_tensor_arr,
                                                                    base_shape_coeff_sz_container,
                                                                    shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                    shape_base_promotion_tag,
                                                                    has_process_unit_logit_reuse_tag);

            for (size_t j = 0u; j < BATCH_SZ; ++j)
            {
                const size_t virtual_idx    = i * BATCH_SZ + j;
                const size_t actual_i       = virtual_idx / tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
                const size_t actual_j       = virtual_idx % tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;

                accum_vec[actual_i].process_vec[actual_j] = out_tensor_arr[j];
            }
        }

        ProcessGroup rs{};

        for (size_t i = 0u; i < tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ; ++i)
        {
            rs.process_vec[i] = tensor_process_unit_operation::avg(accum_vec[i].process_vec.data(), accum_vec[i].process_vec.size());
        }

        return rs;
    }

    constexpr __device__ auto deparameterize(const ProcessGroup& process_group,
                                             double perc) -> ProcessGroup
    {
        size_t tentative_deparam_sz     = process_group.process_vec.size() * perc;
        size_t deparam_sz               = std::clamp(tentative_deparam_sz, size_t{0u}, process_group.process_vec.size());
        size_t active_sz                = process_group.process_vec.size() - deparam_sz;

        std::array<ProcessUnit, PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ> rs{};

        for (size_t i = 0u; i < active_sz; ++i)
        {
            rs[i] = process_group.process_vec[i];
        }

        for (size_t i = 0u; i < deparam_sz; ++i)
        {
            rs[active_sz + i] = cuda_matrix::tensor_process_unit_operation::empty_as(process_group.process_vec[active_sz + i]);
        }

        return 
        {
            .process_vec = rs
        };
    }

    constexpr __device__ auto accumulate(const ProcessGroup& lhs,
                                         const ProcessGroup& rhs) -> ProcessGroup
    {
        const auto& lhs_content = lhs.process_vec;
        const auto& rhs_content = rhs.process_vec;

        ProcessGroup rs{};

        for (size_t i = 0u; i < lhs_content.size(); ++i)
        {
            rs.process_vec[i] = tensor_process_unit_operation::accumulate(lhs_content[i], rhs_content[i]);
        }

        return rs;
    }

    constexpr __device__ auto accumulate(const ProcessGroup * process_group_arr,
                                         size_t process_group_arr_sz) -> ProcessGroup
    {
        if (process_group_arr_sz == 0u)
        {
            assert(false);
        }

        ProcessGroup result = process_group_arr[0];

        for (size_t i = 1u; i < process_group_arr_sz; ++i)
        {
            result = accumulate(result, process_group_arr[i]);
        }

        return result;
    }

    template <class FloatType>
    constexpr __device__ auto positional_encode(const ProcessGroup& process_group,
                                                const FloatType& amplitude,
                                                const FloatType& frequency_multiplier,
                                                const FloatType& x_offset,
                                                const FloatType& y_offset,
                                                size_t dimension_idx) -> ProcessGroup
    {
        ProcessGroup rs             = process_group;
        size_t tentative_slot_idx   = dimension_idx / tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;
        size_t slot_idx             = tentative_slot_idx % tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
        size_t slot_offset          = dimension_idx % tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;

        rs.process_vec[slot_idx]    = tensor_process_unit_operation::positional_encode(rs.process_vec[slot_idx],
                                                                                       amplitude,
                                                                                       frequency_multiplier,
                                                                                       x_offset,
                                                                                       y_offset,
                                                                                       slot_offset);

        return rs;
    }

    template <class ValueType>
    constexpr __device__ auto div(const ProcessGroup& process_group,
                                  const ValueType& value) -> ProcessGroup
    {
        ProcessGroup rs{};

        for (size_t i = 0u; i < rs.process_vec.size(); ++i)
        {
            rs.process_vec[i] = tensor_process_unit_operation::div(process_group.process_vec[i], value);
        }

        return rs;
    }

    constexpr __device__ auto avg(const ProcessGroup * process_group_arr,
                                  size_t process_group_arr_sz) -> ProcessGroup
    {
        return div(accumulate(process_group_arr, process_group_arr_sz),
                   safe_non_zero_access(process_group_arr_sz));
    }
}

#endif