#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_BEING_UNIT_OPERATION_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_BEING_UNIT_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include "tensor_model.h"
#include <stdexcept>
#include "tensor_process_group_operation.h"
#include <array>
#include <cuda_management/device_memory.h>
#include <cuda_management/scope_allocator.h>
#include "local_exception.h"

namespace taylor_matrix::cuda_matrix::tensor_being_unit_operation
{
    using namespace taylor_matrix::cuda_matrix::tensor_model;
    using namespace taylor_matrix::cuda_matrix::utility;
    using namespace taylor_matrix::cuda_matrix::local_exception;

    //--CREATE--

    template <class AllocatorInterface>
    __device__ constexpr auto allocate(size_t process_group_vec_sz,
                                       AllocatorInterface&& allocator) -> BeingUnit *
    {
        using namespace cuda_management::device_memory;

        BeingUnit * rs              = std_new_object<BeingUnit>(allocator);
        ProcessGroup * rs_content   = std_new_array<ProcessGroup>(allocator, process_group_vec_sz);

        *rs = BeingUnit
        {
            .process_group_vec      = rs_content,
            .process_group_vec_sz   = process_group_vec_sz
        };

        return rs;
    }

    template <class AllocatorInterface>
    __device__ constexpr void deallocate(BeingUnit * dst,
                                         AllocatorInterface&& allocator)
    {
        using namespace cuda_management::device_memory;

        safe_ptr_access(dst);

        std_delete_array(allocator, dst->process_group_vec);
        std_delete_object(allocator, dst);
    }

    //--READ--

    __device__ constexpr auto flatten_to(tensor_std_float_t * dst,
                                         const BeingUnit * src) -> tensor_std_float_t *
    {
        safe_ptr_access(src);

        for (size_t i = 0u; i < src->process_group_vec_sz; ++i)
        {
            dst = tensor_process_group_operation::flatten_to(dst, src->process_group_vec[i]);
        }

        return dst;
    }

    __device__ constexpr auto flatten_size(const BeingUnit * src) -> size_t
    {
        safe_ptr_access(src);

        size_t total = 0u;

        for (size_t i = 0u; i < src->process_group_vec_sz; ++i)
        {
            total += tensor_process_group_operation::flatten_size(src->process_group_vec[i]);
        }

        return total;
    }

    //--OPERATION--

    template <class AllocatorInterface>
    __device__ constexpr auto empty_as(const BeingUnit * arg,
                                       AllocatorInterface&& allocator) -> BeingUnit *
    {
        safe_ptr_access(arg);
        BeingUnit * rs = allocate(arg->process_group_vec_sz, allocator);
        safe_ptr_access(rs);

        for (size_t i = 0u; i < rs->process_group_vec_sz; ++i)
        {
            rs->process_group_vec[i] = tensor_process_group_operation::empty_as(arg->process_group_vec[i]);
        }

        return rs;
    }

    template <class AllocatorInterface>
    __device__ constexpr auto copy(const BeingUnit * arg,
                                   AllocatorInterface&& allocator) -> BeingUnit *
    {
        safe_ptr_access(arg);
        BeingUnit * rs = allocate(arg->process_group_vec_sz, allocator);
        safe_ptr_access(rs);

        for (size_t i = 0u; i < arg->process_group_vec_sz; ++i)
        {
            rs->process_group_vec[i] = arg->process_group_vec[i];
        }

        return rs;
    }

    __device__ constexpr void copy_to(BeingUnit * dst,
                                      const BeingUnit * src,
                                      local_exception_t * err = nullptr)
    {
        safe_ptr_access(dst);
        safe_ptr_access(src);

        if (dst->process_group_vec_sz != src->process_group_vec_sz)
        {
            if (err != nullptr)
            {
                *err = OTHER_INVALID_ARGUMENT_CODE;
            }

            return;
        }

        for (size_t i = 0u; i < dst->process_group_vec_sz; ++i)
        {
            dst->process_group_vec[i] = src->process_group_vec[i];
        }
    }

    __device__ constexpr auto unflatten_to(BeingUnit * dst,
                                           const tensor_std_float_t * src) -> const tensor_std_float_t *
    {
        safe_ptr_access(dst);

        for (size_t i = 0u; i < dst->process_group_vec_sz; ++i)
        {
            src = tensor_process_group_operation::unflatten_to(dst->process_group_vec[i], src);
        }

        return src;
    }

    template <class ShapeBaseCoeffSizeContainer,
              class ScopeAllocatorInterface,
              class ShapeBasePromotedFloatType = tensor_model::tensor_std_float_t>
    __device__ constexpr __attribute__((noinline)) auto two_to_one_project_helper(ProcessGroup * process_group,
                                                                                  size_t process_group_sz,
                                                                                  size_t rotation_sz,
                                                                                  ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                                  const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                                  ScopeAllocatorInterface&& allocator,
                                                                                  const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                                  bool has_process_unit_logit_reuse_tag = true,
                                                                                  bool has_process_group_logit_reuse_tag = true,
                                                                                  bool has_being_logit_reuse_tag = true,
                                                                                  local_exception_t * err = nullptr) -> ProcessGroup *
    {
        using namespace cuda_management::scope_allocator;
        using namespace cuda_management::device_memory;

        const std::array<uint64_t, 6u> GROUP_SZ_SET{uint64_t{1} << 1,
                                                    uint64_t{1} << 2,
                                                    uint64_t{1} << 4,
                                                    uint64_t{1} << 8,
                                                    uint64_t{1} << 16,
                                                    uint64_t{1} << 32};

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        safe_ptr_access(process_group);

        if (std::find(process_group_sz, GROUP_SZ_SET.begin(), GROUP_SZ_SET.end()) == GROUP_SZ_SET.end())
        {
            assert(false);
        }

        if (rotation_sz == 0u)
        {
            assert(false);
        }

        ProcessGroup * rs   = std_new_array<ProcessGroup>(allocator, process_group_sz);
        
        scope_guard allocator_grd(&allocator);
        safe_ptr_access(rs);

        for (size_t i = 0u; i < process_group_sz; ++i)
        {
            rs[i] = process_group[i];
        }

        if (process_group_sz == 2u)
        {
            const size_t saved_shape_coeff_arr_offset = shape_coeff_arr_offset;

            ProcessGroup lhs        = tensor_process_group_operation::two_to_one_project(rs[0],
                                                                                         rs[1],
                                                                                         base_shape_coeff_sz_container,
                                                                                         shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                         shape_base_promotion_tag,
                                                                                         has_process_unit_logit_reuse_tag,
                                                                                         has_process_group_logit_reuse_tag,
                                                                                         err);

            if (*err != SUCCESS)
            {
                return nullptr;
            }

            shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;
            ProcessGroup rhs        = tensor_process_group_operation::two_to_one_project(rs[1],
                                                                                         rs[0],
                                                                                         base_shape_coeff_sz_container,
                                                                                         shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                         shape_base_promotion_tag,
                                                                                         has_process_unit_logit_reuse_tag,
                                                                                         has_process_group_logit_reuse_tag,
                                                                                         err);

            if (*err != SUCCESS)
            {
                return nullptr;
            }

            rs[0]   = lhs;
            rs[1]   = rhs;

            return rs;
        }

        for (size_t i = 0u; i < rotation_sz; ++i)
        {
            scope_guard allocator_grd_2(&allocator);

            size_t transpose_sz                         = std::sqrt(process_group_sz);
            ProcessGroup ** rs_sub_matrix               = split_process_group(rs, transpose_sz);
            const size_t saved_shape_coeff_arr_offset   = shape_coeff_arr_offset;

            for (size_t j = 0u; j < transpose_sz; ++i)
            {
                shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;
                rs_sub_matrix[j]        = two_to_one_project_helper(rs_sub_matrix,
                                                                    transpose_sz,
                                                                    rotation_sz,
                                                                    base_shape_coeff_sz_container,
                                                                    shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                    allocator,
                                                                    shape_base_promotion_tag,
                                                                    has_process_unit_logit_reuse_tag,
                                                                    has_process_group_logit_reuse_tag,
                                                                    has_being_logit_reuse_tag,
                                                                    err);

                if (*err != SUCCESS)
                {
                    return nullptr;
                }
            }

            ProcessGroup * tmp = unsplit_process_group(rs_sub_matrix, transpose_sz);

            if (i + 1 != rotation_sz)
            {
                tmp = sqr_transpose_process_group(rs, process_group_sz);
            }

            copy_to(rs, tmp, err);

            if (*err != SUCCESS)
            {
                return nullptr;
            }
        }

        return rs;
    }

    template <class ShapeBaseCoeffSizeContainer,
              class ScopeAllocatorInterface,
              class ShapeBasePromotedFloatType = tensor_model::tensor_std_float_t>
    __device__ constexpr __attribute__((noinline)) auto two_to_one_project(const BeingUnit * lhs,
                                                                           const BeingUnit * rhs,
                                                                           ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                           const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                           ScopeAllocatorInterface&& allocator,
                                                                           const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                           bool has_process_unit_logit_reuse_tag = true,
                                                                           bool has_process_group_logit_reuse_tag = true,
                                                                           bool has_being_logit_reuse_tag = true,
                                                                           local_exception_t * err = nullptr) -> BeingUnit *
    {
        using namespace cuda_management::scope_allocator;
        using namespace cuda_management::device_memory;
        
        local_exception_t local_err = SUCCESS;

        if (err = nullptr)
        {
            err = &local_err;
        }

        safe_ptr_access(lhs);
        safe_ptr_access(rhs);

        BeingUnit * output = allocate(lhs->process_group_vec_sz, allocator);

        safe_ptr_access(output);

        {
            scope_guard _allocator_grd(&allocator);

            ProcessGroup * rs                           = output->process_group_vec;
            const size_t saved_shape_coeff_arr_offset   = shape_coeff_arr_offset;
            ProcessGroup * accum_arr                    = std_new_array<ProcessGroup>(allocator, rhs->process_group_vec_sz);

            for (size_t i = 0u; i < lhs->process_group_vec_sz; ++i)
            {
                if (has_being_logit_reuse_tag)
                {
                    shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;
                }

                for (size_t j = 0u; j < rhs->process_group_vec_sz; ++j)
                {
                    accum_arr[j]                = tensor_process_group_operation::two_to_one_project(lhs->process_group_vec[i],
                                                                                                     rhs->process_group_vec[j],
                                                                                                     base_shape_coeff_sz_container,
                                                                                                     shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                                     shape_base_promotion_tag,
                                                                                                     has_process_unit_logit_reuse_tag,
                                                                                                     has_process_group_logit_reuse_tag,
                                                                                                     err);

                    if (*err != SUCCESS)
                    {
                        return nullptr;
                    }
                }

                rs[i] = tensor_process_group_operation::avg(accum_arr, rhs->process_group_vec_sz, err);

                if (*err != SUCCESS)
                {
                    return nullptr;
                }
            }
        }

        return output;
    }

    template <class AllocatorInterface>
    __device__ constexpr auto deparameterize(const BeingUnit * arg,
                                             double perc,
                                             AllocatorInterface&& allocator) -> BeingUnit *
    {
        safe_ptr_access(arg);
        BeingUnit * rs  = allocate(arg->process_group_vec_sz, allocator);
        safe_ptr_access(rs);

        for (size_t i = 0u; i < arg->process_group_vec_sz; ++i)
        {
            rs->process_group_vec[i] = tensor_process_group_operation::deparameterize(arg->process_group_vec[i], perc);
        }

        return rs;
    }

    template <class AllocatorInterface>
    __device__ constexpr auto accumulate(const BeingUnit * lhs,
                                         const BeingUnit * rhs,
                                         AllocatorInterface&& allocator,
                                         local_exception_t * err = nullptr) -> BeingUnit *
    {
        safe_ptr_access(lhs);
        safe_ptr_access(rhs);

        if (lhs->process_group_vec_sz != rhs->process_group_vec_sz)
        {
            if (err != nullptr)
            {
                *err = OTHER_INVALID_ARGUMENT_CODE;
            }

            return nullptr;
        }

        BeingUnit * rs  = allocate(lhs->process_group_vec_sz, allocator);        
        safe_ptr_access(rs);

        for (size_t i = 0u; i < lhs->process_group_vec_sz; ++i)
        {
            rs->process_group_vec[i] = tensor_process_group_operation::accumulate(lhs->process_group_vec[i], rhs->process_group_vec[i]);
        }

        return rs;
    }

    template <class ScopeAllocatorInterface>
    __device__ constexpr auto accumulate(BeingUnit ** arg_arr,
                                         size_t arg_arr_sz,
                                         ScopeAllocatorInterface&& allocator,
                                         local_exception_t * err = nullptr) -> BeingUnit *
    {
        using namespace cuda_management::scope_allocator;

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        if (arg_arr_sz == 0u)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;

            return {};
        }

        BeingUnit * rs = copy(arg_arr[0], allocator);

        for (size_t i = 1u; i < arg_arr_sz; ++i)
        {
            scope_guard scope_grd(&allocator);

            BeingUnit * tmp = accumulate(rs, arg_arr[i], allocator, err);
            
            if (*err != SUCCESS)
            {
                return {};
            }

            copy_to(rs, tmp, err);

            if (*err != SUCCESS)
            {
                return {};
            }
        }

        return rs;
    }

    template <class ValueType, class AllocatorInterface>
    __device__ constexpr auto div(const BeingUnit * arg,
                                  const ValueType& value,
                                  AllocatorInterface&& allocator) -> BeingUnit *
    {
        safe_ptr_access(arg);
        BeingUnit * rs = allocate(arg->process_group_vec_sz, allocator);
        safe_ptr_access(rs);

        for (size_t i = 0u; i < arg->process_group_vec_sz; ++i)
        {
            rs->process_group_vec[i] = tensor_process_group_operation::div(arg->process_group_vec[i], value);
        }

        return rs;
    }

    template <class ScopeAllocatorInterface>
    __device__ constexpr auto avg(BeingUnit ** arg_arr,
                                  size_t arg_arr_sz,
                                  ScopeAllocatorInterface&& allocator,
                                  local_exception_t * err = nullptr) -> BeingUnit *
    {
        BeingUnit * accum_rs = accumulate(arg_arr, arg_arr_sz, allocator, err);

        if (accum_rs == nullptr)
        {
            return nullptr;
        }

        return div(accum_rs, arg_arr_sz, allocator);
    }
}

#endif