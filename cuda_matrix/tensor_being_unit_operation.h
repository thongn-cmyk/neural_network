#ifndef __CUDA_MATRIX_TENSOR_BEING_UNIT_OPERATION_H__
#define __CUDA_MATRIX_TENSOR_BEING_UNIT_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include "tensor_model.h"
#include <stdexcept>
#include "tensor_process_group_operation.h"
#include <array>

namespace cuda_matrix::tensor_being_unit_operation
{
    using namespace cuda_matrix::tensor_model;
    using namespace cuda_matrix::utility;

    constexpr __device__ void empty_as(const BeingUnit& arg,
                                       BeingUnit& rs)
    {
        if (arg.process_group_vec_sz != rs.process_group_vec_sz)
        {
            assert(false);
        }

        for (size_t i = 0u; i < rs.process_group_vec_sz; ++i)
        {
            rs.process_group_vec[i] = tensor_process_group_operation::empty_as(arg.process_group_vec[i]);
        }
    }

    constexpr __device__ void copy_to(BeingUnit& dst,
                                      const BeingUnit& src)
    {
        if (dst.process_group_vec_sz != src.process_group_vec_sz)
        {
            assert(false);
        }

        for (size_t i = 0u; i < src.process_group_vec_sz; ++i)
        {
            dst.process_group_vec[i] = src.process_group_vec[i];
        }
    }

    constexpr __device__ void allocate(size_t process_group_vec_sz,
                                       AllocatorInterface& allocator,
                                       BeingUnit ** dst)
    {
        if (dst == nullptr)
        {
            return;
        }

        BeingUnit * container   = allocate_object<BeingUnit>(allocator);
        ProcessGroup * rs_arr   = allocate_array<ProcessGroup>(allocator, process_group_vec_sz);

        *container              = BeingUnit
        {
            .process_group_vec      = rs_arr,
            .process_group_vec_sz   = process_group_vec_sz
        };

        *dst                    = container;
    }

    constexpr __device__ void deallocate(BeingUnit * dst,
                                         AllocatorInterface& allocator)
    {
        if (dst == nullptr)
        {
            assert(false);
        }

        deallocate_array(allocator, dst.process_group_vec);
        deallocate_object(allocator, dst);
    }

    template <class ShapeBaseCoeffSizeContainer,
              class ShapeBasePromotedFloatType = tensor_model::tensor_std_float_t>
    constexpr __device__ __attribute__((noinline)) void two_to_one_project(const BeingUnit& lhs,
                                                                           const BeingUnit& rhs,
                                                                           BeingUnit& output,
                                                                           ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                           const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                           ScopeAllocatorInterface& scope_allocator,
                                                                           const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                           bool has_process_unit_logit_reuse_tag = true,
                                                                           bool has_process_group_logit_reuse_tag = true,
                                                                           bool has_being_logit_reuse_tag = true)
    {
        scope_guard<ScopeAllocatorInterface> _allocator_grd(scope_allocator);

        if (lhs.process_group_vec_sz != rs.process_group_vec_sz)
        {
            assert(false);
        }

        ProcessGroup * rs                           = output.process_group_vec;
        const size_t saved_shape_coeff_arr_offset   = shape_coeff_arr_offset;
        ProcessGroup * accum_arr                    = allocate_array<ProcessGroup>(scope_allocator, rhs.process_group_vec_sz);

        for (size_t i = 0u; i < lhs.process_group_vec_sz; ++i)
        {
            if (has_being_logit_reuse_tag)
            {
                shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;
            }

            for (size_t j = 0u; j < rhs.process_group_vec_sz; ++j)
            {
                accum_arr[j]     = tensor_process_group_operation::two_to_one_project(lhs.process_group_vec[i],
                                                                                      rhs.process_group_vec[j],
                                                                                      base_shape_coeff_sz_container,
                                                                                      shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                      shape_base_promotion_tag,
                                                                                      has_process_unit_logit_reuse_tag,
                                                                                      has_process_group_logit_reuse_tag);
            }

            rs[i] = tensor_process_group_operation::avg(accum_arr, rhs.process_group_vec_sz);
        }
    }

    constexpr __device__ void deparameterize(const BeingUnit& arg,
                                             BeingUnit& rs,
                                             double perc)
    {
        if (arg.process_group_vec_sz != rs.process_group_vec_sz)
        {
            assert(false);
        }

        for (size_t i = 0u; i < arg.process_group_vec_sz; ++i)
        {
            rs.process_group_vec[i] = tensor_process_group_operation::deparameterize(arg.process_group_vec[i], perc);
        }
    }

    constexpr __device__ void accumulate(const BeingUnit& lhs,
                                         const BeingUnit& rhs,
                                         BeingUnit& rs)
    {
        if (lhs.process_group_vec_sz != rhs.process_group_vec_sz)
        {
            assert(false);
        }

        for (size_t i = 0u; i < lhs.process_group_vec_sz; ++i)
        {
            rs.process_group_vec[i] = tensor_process_group_operation::accumulate(lhs.process_group_vec[i], rhs.process_group_vec[i]);
        }
    }

    constexpr __device__ void accumulate(BeingUnit ** arg_arr,
                                         size_t arg_arr_sz,
                                         BeingUnit& rs)
    {
        if (arg_arr_sz == 0u)
        {
            assert(false);
        }

        copy_to(rs, &arg_arr[0]);

        for (size_t i = 1u; i < being_arr_sz; ++i)
        {
            accumulate(rs, *arg_arr[i], rs);
        }
    }

    template <class FloatType>
    constexpr __device__ void positional_encode(const BeingUnit& arg,
                                                BeingUnit& rs,
                                                const FloatType& amplitude,
                                                const FloatType& frequency_multiplier,
                                                const FloatType& x_offset,
                                                const FloatType& y_offset,
                                                size_t pe_dimension_idx,
                                                size_t dedicated_pe_sz)
    {
        if (arg.process_group_vec_sz != rs.process_group_vec_sz)
        {
            assert(false);
        }

        size_t actual_pe_sz = std::min(arg.process_group_vec_sz, dedicated_pe_sz);

        for (size_t i = 0u; i < arg.process_group_vec_sz; ++i)
        {
            if (i < actual_pe_sz)
            {
                rs.process_group_vec[i] = tensor_process_group_operation::positional_encode(arg.process_group_vec[i],
                                                                                            amplitude,
                                                                                            frequency_multiplier,
                                                                                            x_offset,
                                                                                            y_offset,
                                                                                            pe_dimension_idx);
            }
            else
            {
                rs.process_group_vec[i] = arg.process_group_vec[i];
            }
        }
    }

    template <class ValueType>
    constexpr __device__ void div(const BeingUnit& arg,
                                  const ValueType& value,
                                  BeingUnit& rs)
    {
        if (arg.process_group_vec_sz != rs.process_group_vec_sz)
        {
            assert(false);
        }

        for (size_t i = 0u; i < arg.process_group_vec_sz; ++i)
        {
            rs.process_group_vec[i] = tensor_process_group_operation::div(arg.process_group_vec[i], value);
        }
    }

    constexpr __device__ void avg(BeingUnit ** arg_arr,
                                  size_t arg_arr_sz,
                                  BeingUnit& rs)
    {
        accumulate(arg_arr, arg_arr_sz, rs);
        div(rs, arg_arr_sz, rs);
    }
}

#endif