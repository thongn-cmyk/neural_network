#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_MATRIX_OPERATION_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_MATRIX_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include "tensor_model.h"
#include <stdexcept>
#include "dispatch_code_generator.h"
#include "tensor_being_unit_operation.h"
#include <array>
#include <cuda_management/scope_allocator.h>
#include "local_exception.h"

namespace taylor_matrix::cuda_matrix::tensor_matrix_operation
{
    using namespace taylor_matrix::cuda_matrix::tensor_model;
    using namespace taylor_matrix::cuda_matrix::utility;
    using namespace taylor_matrix::cuda_matrix::local_exception;

    using DispatchCodeGenerator = taylor_matrix::cuda_matrix::dispatch_code_generator::DispatchCodeGenerator;

    //--CREATE--

    template <class AllocatorInterface>
    __device__ constexpr auto allocate(size_t being_vec_sz,
                                       size_t process_group_vec_sz,
                                       AllocatorInterface&& allocator) -> Matrix *
    {
        using namespace cuda_management::device_memory; 

        Matrix * result         = std_new_object<Matrix>(allocator);
        BeingUnit ** being_vec  = std_new_array<std::add_pointer_t<BeingUnit>>(allocator, being_vec_sz);

        for (size_t i = 0u; i < being_vec_sz; ++i)
        {
            being_vec[i]    = taylor_matrix::cuda_matrix::tensor_being_unit_operation::allocate(process_group_vec_sz, allocator);
        }

        result->being_vec       = being_vec;
        result->being_vec_sz    = being_vec_sz;

        return result;
    }

    template <class AllocatorInterface>
    __device__ constexpr void deallocate(Matrix * arg,
                                         AllocatorInterface&& allocator) noexcept
    {
        using namespace cuda_management::device_memory;

        safe_ptr_access(arg);

        for (size_t i = 0u; i < arg->being_vec_sz; ++i)
        {
            taylor_matrix::cuda_matrix::tensor_being_unit_operation::deallocate(arg->being_vec[i], allocator);
        }

        std_delete_array(allocator, arg->being_vec);
        std_delete_object(allocator, arg);
    }

    //--READ--

    __device__ constexpr auto flatten_to(tensor_std_float_t * dst,
                                         const Matrix * src) -> tensor_std_float_t *
    {
        safe_ptr_access(src);

        for (size_t i = 0u; i < src->being_vec_sz; ++i)
        {
            dst = taylor_matrix::cuda_matrix::tensor_being_unit_operation::flatten_to(dst, src->being_vec[i]);
        }

        return dst;
    }

    __device__ constexpr auto flatten_size(const Matrix * arg) -> size_t
    {
        safe_ptr_access(arg);
        size_t sz = 0u;

        for (size_t i = 0u; i < arg->being_vec_sz; ++i)
        {
            sz += taylor_matrix::cuda_matrix::tensor_being_unit_operation::flatten_size(arg->being_vec[i]);
        }

        return sz;
    }

    __device__ constexpr auto unflatten_to(Matrix * dst,
                                           const tensor_std_float_t * src) -> const tensor_std_float_t *
    {
        safe_ptr_access(dst);

        for (size_t i = 0u; i < dst->being_vec_sz; ++i)
        {
            src = taylor_matrix::cuda_matrix::tensor_being_unit_operation::unflatten_to(dst->being_vec[i], src);
        }

        return src;
    }

    __device__ constexpr auto check_shape(size_t * shape_arr,
                                          size_t shape_arr_sz) -> local_exception_t
    {
        if (shape_arr_sz > 4u)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }

        if (0u < shape_arr_sz)
        {
            if (shape_arr[0u] == 0u)
            {
                return OTHER_INVALID_ARGUMENT_CODE;
            }
        }

        if (1u < shape_arr_sz)
        {
            if (shape_arr[1u] == 0u)
            {
                return OTHER_INVALID_ARGUMENT_CODE;
            }
        }

        if (2u < shape_arr_sz)
        {
            if (shape_arr[2u] != device_tensor::model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ)
            {
                return OTHER_INVALID_ARGUMENT_CODE;
            }
        }

        if (3u < shape_arr_sz )
        {
            if (shape_arr[3u] != device_tensor::model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ)
            {
                return OTHER_INVALID_ARGUMENT_CODE;
            }
        }

        return SUCCESS;
    }

    //--OPERATION--

    __device__ constexpr void copy_to(Matrix * dst,
                                      const Matrix * src,
                                      local_exception_t * err = nullptr)
    {
        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        safe_ptr_access(dst);
        safe_ptr_access(src);

        if (dst->being_vec_sz != src->being_vec_sz)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;

            return false;
        }

        for (size_t i = 0u; i < src->being_vec_sz; ++i)
        {
            taylor_matrix::cuda_matrix::tensor_being_unit_operation::copy_to(dst->being_vec[i],
                                                                             src->being_vec[i],
                                                                             err);

            if (*err != SUCCESS)
            {
                return;
            }
        }
    }

    //scope-operation
    template <class AllocatorInterface> 
    __device__ constexpr auto copy(const Matrix * arg,
                                   AllocatorInterface&& allocator) -> Matrix *
    {
        using namespace cuda_management::device_memory;

        safe_ptr_access(arg);

        Matrix * rs         = std_new_object<Matrix>(allocator);
        rs->being_vec       = std_new_array<std::add_pointer_t<BeingUnit>>(allocator, arg->being_vec_sz); 
        rs->being_vec_sz    = arg->being_vec_sz;

        for (size_t i = 0u; i < arg->being_vec_sz; ++i)
        {
            rs->being_vec[i] = taylor_matrix::cuda_matrix::tensor_being_unit_operation::copy(arg->being_vec[i], allocator);
        }

        return rs;
    }

    //scope-operation, lifetime of Matrix * matrix
    template <class SuffixMap, class AllocatorInterface>
    __device__ constexpr auto matrix_to_focal(Matrix * matrix,
                                              size_t i,
                                              SuffixMap focal_suffix_map, /*inplace_unordered_map<size_t, inplace_unordered_map<size_t, inplace_vector<inplace_vector<size_t>>>>*/
                                              AllocatorInterface&& allocator,
                                              local_exception_t * err = nullptr) -> std::pair<Matrix **, size_t>
    {
        using namespace cuda_management::device_memory;

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        safe_ptr_access(matrix);

        auto map_ptr = focal_suffix_map.find(matrix->being_vec_sz);

        if (map_ptr == focal_suffix_map.end())
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        auto map_ptr2 = (*map_ptr).second.find(i);

        if (map_ptr2 == (*map_ptr).second.end())
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        auto focal_dictionary   = (*map_ptr2).second;

        size_t focal_sz         = focal_dictionary.size();
        Matrix ** rs            = std_new_array<std::add_pointer_t<Matrix>>(allocator, focal_sz);

        for (size_t i = 0u ; i < focal_sz; ++i)
        {
            rs[i]               = std_new_object<Matrix>(allocator);
            rs[i]->being_vec    = std_new_array<std::add_pointer_t<BeingUnit>>(allocator, matrix->being_vec_sz);
            rs[i]->being_vec_sz = matrix->being_vec_sz;
        }

        size_t suffix_arr_idx   = 0u;

        for (auto suffix_arr: focal_dictionary)
        {
            for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
            {
                if (i >= suffix_arr.size())
                {
                    *err = OTHER_INVALID_ARGUMENT_CODE;
                    return {};
                }

                size_t dst_idx  = suffix_arr[i];
                size_t src_idx  = i;

                if (dst_idx >= rs[suffix_arr_idx]->being_vec_sz)
                {
                    *err = OTHER_INVALID_ARGUMENT_CODE;
                    return {};
                }

                rs[suffix_arr_idx]->being_vec[dst_idx]  = matrix->being_vec[src_idx]; 
            }

            suffix_arr_idx += 1;
        }

        return {rs, focal_sz};
    }

    //scope-operation, lifetime of Matrix * matrix
    template <class AllocatorInterface>
    __device__ constexpr auto focal_split_matrix(Matrix * matrix,
                                                 size_t group_by_sz,
                                                 AllocatorInterface&& allocator,
                                                 local_exception_t * err = nullptr) -> Matrix **
    {
        using namespace cuda_management::device_memory;

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        if (group_by_sz == 0u)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        if (matrix->being_vec_sz % group_by_sz != 0u)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        Matrix ** rs    = std_new_array<std::add_pointer_t<Matrix>>(allocator, group_by_sz);
        size_t focal_sz = matrix->being_vec_sz / group_by_sz;

        for (size_t i = 0u; i < group_by_sz; ++i)
        {
            size_t first        = i * focal_sz;
            size_t last         = static_cast<size_t>((i + 1) * focal_sz);
            Matrix * sub_matrix = std_new_object<Matrix>(allocator);

            *sub_matrix         = Matrix
            {
                .being_vec      = utility::next(matrix->being_vec, first),
                .being_vec_sz   = last - first
            };

            rs[i]               = sub_matrix;
        }

        return rs;
    }

    //scope-operation, lifetime of Matrix ** matrix_vec, size_t matrix_vec_sz
    template <class AllocatorInterface>
    __device__ constexpr auto focal_unsplit_matrix(Matrix ** matrix_vec, size_t matrix_vec_sz,
                                                   AllocatorInterface&& allocator) -> Matrix *
    {
        using namespace cuda_management::device_memory;

        size_t being_vec_sz     = 0u;

        for (size_t i = 0u; i < matrix_vec_sz; ++i)
        {
            being_vec_sz += safe_ptr_access(matrix_vec[i])->being_vec_sz;
        }

        BeingUnit ** being_vec  = std_new_array<std::add_pointer_t<BeingUnit>>(allocator, being_vec_sz);
        size_t ptr              = 0u; 

        for (size_t i = 0u; i < matrix_vec_sz; ++i)
        {
            for (size_t j = 0u; j < matrix_vec[i]->being_vec_sz; ++j)
            {
                being_vec[ptr++] = matrix_vec[i]->being_vec[j];
            }
        }

        Matrix * rs             = std_new_object<Matrix>(allocator);

        rs->being_vec           = being_vec;
        rs->being_vec_sz        = being_vec_sz;

        return rs;
    }

    //scope-operation
    template <class AllocatorInterface>
    __device__ constexpr auto deparameterize(const Matrix * matrix,
                                             double perc,
                                             AllocatorInterface&& allocator) -> Matrix *
    {
        using namespace cuda_management::device_memory;

        safe_ptr_access(matrix);

        BeingUnit ** rs     = std_new_array<std::add_pointer_t<BeingUnit>>(allocator, matrix->being_vec_sz);

        for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
        {
            rs[i] = tensor_being_unit_operation::deparameterize(matrix->being_vec[i], perc, allocator);
        }

        Matrix * obj        = std_new_object<Matrix>(allocator); 
        obj->being_vec      = rs;
        obj->being_vec_sz   = matrix->being_vec_sz;

        return obj;
    }

    //scope-operation
    template <class AllocatorInterface>
    __device__ constexpr auto accumulate(const Matrix * lhs,
                                         const Matrix * rhs,
                                         AllocatorInterface&& allocator,
                                         local_exception_t * err = nullptr) -> Matrix *
    {
        using namespace cuda_management::device_memory;

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        safe_ptr_access(lhs);
        safe_ptr_access(rhs);

        if (lhs->being_vec_sz != rhs->being_vec_sz)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        Matrix * rs             = std_new_object<Matrix>(allocator);
        BeingUnit ** content    = std_new_array<std::add_pointer_t<BeingUnit>>(allocator, lhs->being_vec_sz);

        rs->being_vec           = content;
        rs->being_vec_sz        = lhs->being_vec_sz;

        for (size_t i = 0u; i < rs->being_vec_sz; ++i)
        {
            rs->being_vec[i]    = tensor_being_unit_operation::accumulate(lhs->being_vec[i], rhs->being_vec[i], allocator, err);

            if (*err != SUCCESS)
            {
                return {};
            }
        }

        return rs;
    }

    //scope-operation
    template <class AllocatorInterface>
    __device__ constexpr auto accumulate(Matrix ** matrix_arr,
                                         size_t matrix_arr_sz,
                                         AllocatorInterface&& allocator,
                                         local_exception_t * err = nullptr) -> Matrix *
    {
        using namespace cuda_management::device_memory;
        using namespace cuda_management::scope_allocator;

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        if (matrix_arr_sz == 0u)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        Matrix * rs = copy(matrix_arr[0], allocator);

        for (size_t i = 1u; i < matrix_arr_sz; ++i)
        {
            scope_guard scope_grd(&allocator);

            Matrix * tmp = accumulate(rs, matrix_arr[i], allocator, err);

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

    //scope-operation
    template <class ValueType,
              class AllocatorInterface>
    __device__ constexpr auto div(const Matrix * matrix,
                                  const ValueType& value,
                                  AllocatorInterface&& allocator) -> Matrix *
    {
        using namespace cuda_management::device_memory;

        safe_ptr_access(matrix);

        Matrix * rs             = std_new_object<Matrix>(allocator);
        BeingUnit ** content    = std_new_array<std::add_pointer_t<BeingUnit>>(allocator, matrix->being_vec_sz);

        rs->being_vec           = content;
        rs->being_vec_sz        = matrix->being_vec_sz;

        for (size_t i = 0u; i < rs->being_vec_sz; ++i)
        {
            rs->being_vec[i]    = tensor_being_unit_operation::div(matrix->being_vec[i],
                                                                   value,
                                                                   allocator);
        }

        return rs;
    }

    //scope-operation
    template <class AllocatorInterface>
    __device__ constexpr auto avg(Matrix ** matrix_arr,
                                  size_t matrix_arr_sz,
                                  AllocatorInterface&& allocator,
                                  local_exception_t * err = nullptr) -> Matrix *
    {
        using namespace cuda_management::device_memory;

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        Matrix * tmp = accumulate(matrix_arr, matrix_arr_sz, allocator, err);

        if (*err != SUCCESS)
        {
            return {};
        }

        return div(tmp, matrix_arr_sz, allocator);
    }

    //scope-operation
    template <class AllocatorInterface>
    __device__ constexpr auto series_normalize(Matrix ** matrix_arr,
                                               size_t matrix_arr_sz,
                                               AllocatorInterface&& allocator,
                                               local_exception_t * err = nullptr) -> Matrix *
    {
        using namespace cuda_management::device_memory;

        constexpr double RATIO_EXP_BASE = 10;

        Matrix ** e_normed_arr  = std_new_array<std::add_pointer_t<Matrix>>(allocator, matrix_arr_sz);
        double current_ratio    = 1u;

        for (size_t i = 0u; i < matrix_arr_sz; ++i)
        {
            e_normed_arr[i] = div(matrix_arr[i], current_ratio, allocator);
            current_ratio   *= RATIO_EXP_BASE;
        }

        return accumulate(e_normed_arr, matrix_arr_sz, allocator, err);
    }

    //scope-operation
    template <class AllocatorInterface, class SuffixMap>
    __device__ constexpr auto unfocal_matrix(Matrix ** matrix_vec, size_t matrix_vec_sz,
                                             size_t i,
                                             SuffixMap focal_suffix_map,  /*inplace_unordered_map<size_t, inplace_unordered_map<size_t, inplace_vector<inplace_vector<size_t>>>>*/
                                             AllocatorInterface&& allocator,
                                             local_exception_t * err = nullptr) -> Matrix *
    {
        using namespace cuda_management::device_memory;

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        if (matrix_vec_sz == 0u)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        auto map_ptr = focal_suffix_map.find(matrix_vec[0]->being_vec_sz);

        if (map_ptr == focal_suffix_map.end())
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        auto map_ptr2 = (*map_ptr).second.find(i);

        if (map_ptr2 == (*map_ptr).second.end())
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        auto focal_dictionary   = (*map_ptr2).second;
        size_t focal_sz         = focal_dictionary.size(); 

        if (matrix_vec_sz != focal_sz)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        Matrix ** result_vec    = std_new_array<std::add_pointer_t<Matrix>>(allocator, focal_sz);

        for (size_t i = 0u; i < focal_sz; ++i)
        {
            if (i >= focal_dictionary.size())
            {
                *err = OTHER_INVALID_ARGUMENT_CODE;
                return {};
            }

            auto suffix_arr         = focal_dictionary[i];
            BeingUnit ** org_arr    = std_new_array<std::add_pointer_t<BeingUnit>>(allocator,
                                                                                   safe_ptr_access(matrix_vec[i])->being_vec_sz);

            for (size_t j = 0u; j < matrix_vec[i]->being_vec_sz; ++j)
            {
                if (j >= suffix_arr.size())
                {
                    *err = OTHER_INVALID_ARGUMENT_CODE;
                    return {};
                }

                size_t suffix   = suffix_arr[j];

                if (suffix >= matrix_vec[i]->being_vec_sz)
                {
                    *err = OTHER_INVALID_ARGUMENT_CODE;
                    return {};
                }

                org_arr[j]      = matrix_vec[i]->being_vec[suffix];
            }

            result_vec[i]   = std_new_object<Matrix>(allocator);
            *result_vec[i]  = Matrix
            {
                .being_vec      = org_arr,
                .being_vec_sz   = matrix_vec[i]->being_vec_sz
            };
        }

        return avg(result_vec, focal_sz, allocator, err);
    }

    template <class AllocatorInterface,
              class ShapeBaseCoeffSizeContainer,
              class ShapeBasePromotedFloatType = tensor_std_float_t>
    __device__ constexpr __attribute__((noinline)) auto mono_transform(Matrix * matrix,
                                                                       ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                       const tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                       AllocatorInterface&& allocator,
                                                                       const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                       local_exception_t * err = nullptr) -> Matrix *
    {
        safe_ptr_access(matrix);

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        Matrix * rs = copy(matrix, allocator);

        for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
        {
            rs->being_vec[i]    = tensor_being_unit_operation::mono_transform(matrix->being_vec[i],
                                                                              base_shape_coeff_sz_container,
                                                                              shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                              allocator,
                                                                              shape_base_promotion_tag,
                                                                              err);

            if (*err !+ SUCCESS)
            {
                return {};
            }
        }

        return rs;
    }

    template <class AllocatorInterface,
              class ShapeBaseCoeffSizeContainer,
              class ShapeBasePromotedFloatType = tensor_std_float_t>
    __device__ constexpr __attribute__((noinline)) auto feed_forward_transform(Matrix * matrix,
                                                                               ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                               const tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                               AllocatorInterface&& allocator,
                                                                               const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                               local_exception_t * err = nullptr) -> Matrix *
    {
        Matrix * mono_matrix    = mono_transform(matrix,
                                                 base_shape_coeff_sz_container,
                                                 shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                 allocator,
                                                 shape_base_promotion_tag,
                                                 err);

        if (*err != SUCCESS)
        {
            return {};
        }

        Matrix ** tmp_rs_arr[]{matrix, mono_matrix};

        return avg(tmp_rs_arr, 2u, allocator, err);
    }

    //this should suffice
    //I know there are a lot of "assumptions" in the arguments that this function should not expose
    //but these are the optimizables that are internally coupled with the factory decisions, in the sense, this is against the practice without the factory

    template <class FocalSizeVector, /*inplace_vector<size_t>*/
              class SuffixMap, /*inplace_unordered_map<size_t, inplace_unordered_map<size_t, inplace_vector<inplace_vector<size_t>>?*/
              class RotationSizeVector, /*inplace_vector<size_t>*/
              class ParameterBoundRatioVector, /*inplace_vector<double>*/
              class ShapeBaseCoeffSizeContainer,
              class AllocatorInterface,
              class ShapeBasePromotedFloatType = tensor_std_float_t>
    __device__ constexpr __attribute__((noinline)) auto matrix_transform(Matrix * matrix,

                                                                         FocalSizeVector focal_sz_vec, size_t focal_sz_vec_offset,
                                                                         SuffixMap focal_suffix_map,

                                                                         RotationSizeVector rotation_sz_vec, size_t rotation_sz_vec_offset,
                                                                         ParameterBoundRatioVector parameter_bound_ratio_vec, size_t parameter_bound_ratio_vec_offset,

                                                                         ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                         const std::add_pointer_t<tensor_std_float_t> * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,

                                                                         DispatchCodeGenerator& dispatch_code_generator,
                                                                         AllocatorInterface&& allocator,

                                                                         local_exception_t * err = nullptr,
                                                                         const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                         bool has_logit_unit_reuse_tag = true,
                                                                         bool has_logit_group_logit_reuse_tag = true,
                                                                         bool has_being_logit_reuse_tag = true,
                                                                         bool has_base_matrix_logit_reuse_tag = true) -> Matrix *
    {
        using namespace cuda_management::scope_allocator;
        using namespace cuda_management::device_memory; 

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        Matrix * rs = copy(safe_ptr_access(matrix), allocator); //
        scope_guard scope_grd(&allocator); //

        if (matrix->being_vec_sz == 0u)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        if (matrix->being_vec_sz == 1u)
        {
            return rs;
        }

        if (matrix->being_vec_sz == 2u)
        {
            const size_t saved_shape_coeff_arr_offset   = shape_coeff_arr_offset;

            BeingUnit * lhs = tensor_being_unit_operation::two_to_one_project(matrix->being_vec[0],
                                                                              matrix->being_vec[1],
                                                                              base_shape_coeff_sz_container,
                                                                              shape_coeff_arr[dispatch_code_generator.get_dispatch_code()], shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                              allocator,
                                                                              shape_base_promotion_tag,
                                                                              has_logit_unit_reuse_tag,
                                                                              has_logit_group_logit_reuse_tag,
                                                                              has_being_logit_reuse_tag,
                                                                              err);
            
            if (*err != SUCCESS)
            {
                return {};
            }

            BeingUnit * enhanced_lhs    = tensor_being_unit_operation::accumulate(lhs,
                                                                                  matrix->being_vec[0],
                                                                                  allocator,
                                                                                  err);

            if (*err !+ SUCCESS)
            {
                return {};
            }

            if (has_base_matrix_logit_reuse_tag)
            {
                shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;
            }

            BeingUnit * rhs = tensor_being_unit_operation::two_to_one_project(matrix->being_vec[1],
                                                                              matrix->being_vec[0],
                                                                              base_shape_coeff_sz_container,
                                                                              shape_coeff_arr[dispatch_code_generator.get_dispatch_code()], shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                              allocator,
                                                                              shape_base_promotion_tag,
                                                                              has_logit_unit_reuse_tag,
                                                                              has_logit_group_logit_reuse_tag,
                                                                              has_being_logit_reuse_tag,
                                                                              err);

            if (*err != SUCCESS)
            {
                return {};
            }

            BeingUnit * enhanced_rhs    = tensor_being_unit_operation::accumulate(rhs,
                                                                                  matrix->being_vec[1],
                                                                                  allocator,
                                                                                  err);

            if (*err != SUCCESS)
            {
                return {};
            }

            std::add_pointer_t<BeingUnit> tmp_being_vec[]{enhanced_lhs, enhanced_rhs};

            Matrix tmp_matrix
            {
                .being_vec      = tmp_being_vec,
                .being_vec_sz   = static_cast<uint64_t>(2)
            };

            Matrix * final_rs    = feed_forward_transform(&tmp_matrix,
                                                          base_shape_coeff_sz_container,
                                                          shape_coeff_arr[dispatch_code_generator.get_dispatch_code()], shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                          allocator,
                                                          shape_base_promotion_tag,
                                                          err);

            copy_to(rs, final_rs, err);

            return rs;
        }

        if (focal_sz_vec.size() == focal_sz_vec_offset)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        if (rotation_sz_vec.size() == rotation_sz_vec_offset)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        if (parameter_bound_ratio_vec.size() == parameter_bound_ratio_vec_offset)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        const size_t focal_sz                       = focal_sz_vec[focal_sz_vec_offset];
        const size_t rotation_sz                    = rotation_sz_vec[rotation_sz_vec_offset];
        const size_t INCREMENTAL_MATRIX_VEC_OFFSET  = 1u;
        const double parameter_bound_ratio          = parameter_bound_ratio_vec[parameter_bound_ratio_vec_offset];

        Matrix * up_to_point_matrix                 = copy(matrix, allocator);
        size_t incremental_matrix_vec_sz            = rotation_sz;
        Matrix ** incremental_matrix_vec            = std_new_array<std::add_pointer_t<Matrix>>(allocator, incremental_matrix_vec_sz);

        for (size_t i = 0u; i < incremental_matrix_vec_sz; ++i)
        {
            incremental_matrix_vec[i] = copy(matrix, allocator);
        }

        for (size_t i = 0u; i < rotation_sz; ++i)
        {
            scope_guard scope_grd(&allocator);

            if (i != 0u)
            {
                Matrix ** focused_matrix_arr_1      = {};
                size_t focused_matrix_vec_sz_1      = {};

                std::tie(focused_matrix_arr_1, focused_matrix_vec_sz_1) = matrix_to_focal(deparameterize(up_to_point_matrix, parameter_bound_ratio, allocator),
                                                                                          i,
                                                                                          focal_suffix_map,
                                                                                          allocator,
                                                                                          err);

                if (*err != SUCCESS)
                {
                    return {};
                }

                Matrix ** accum_incremental_matrix_vec                  = std_new_array<std::add_pointer_t<Matrix>>(allocator, focused_matrix_vec_sz_1);

                for (size_t j = 0u; j < focused_matrix_vec_sz_1; ++j)
                {
                    Matrix * focused_matrix                     = focused_matrix_arr_1[j];
                    Matrix ** focal_matrix_vec                  = focal_split_matrix(focused_matrix, focal_sz, allocator, err);

                    if (*err != SUCCESS)
                    {
                        return {};
                    }

                    Matrix ** transformed_focal_vec             = std_new_array<std::add_pointer_t<Matrix>>(allocator, focal_sz);
                    const size_t saved_shape_coeff_arr_offset_0 = shape_coeff_arr_offset;

                    for (size_t focal_idx = 0u; focal_idx < focal_sz; ++focal_idx)
                    {
                        shape_coeff_arr_offset      = saved_shape_coeff_arr_offset_0;
                        Matrix * focal              = focal_matrix_vec[focal_idx];

                        Matrix * transformed_focal  = matrix_transform(focal,
                                                                       focal_sz_vec, focal_sz_vec_offset + 1,
                                                                       focal_suffix_map,
                                                                       rotation_sz_vec, rotation_sz_vec_offset + 1,
                                                                       parameter_bound_ratio_vec, parameter_bound_ratio_vec_offset + 1,
                                                                       base_shape_coeff_sz_container,
                                                                       shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                       dispatch_code_generator,
                                                                       allocator,
                                                                       err,
                                                                       shape_base_promotion_tag,
                                                                       has_logit_unit_reuse_tag,
                                                                       has_logit_group_logit_reuse_tag,
                                                                       has_being_logit_reuse_tag,
                                                                       has_base_matrix_logit_reuse_tag);

                        if (*err != SUCCESS)
                        {
                            return {};
                        }

                        transformed_focal_vec[focal_idx] = transformed_focal;
                    }

                    Matrix * transformed_focused_matrix = focal_unsplit_matrix(transformed_focal_vec, focal_sz, allocator);
                    accum_incremental_matrix_vec[j]     = transformed_focused_matrix;
                }

                Matrix * incremental_result = unfocal_matrix(accum_incremental_matrix_vec, focused_matrix_vec_sz_1,
                                                             i,
                                                             focal_suffix_map,
                                                             allocator,
                                                             err);

                if (*err != SUCCESS)
                {
                    return {};
                }

                copy_to(incremental_matrix_vec[i + INCREMENTAL_MATRIX_VEC_OFFSET], incremental_result, err);

                if (*err != SUCCESS)
                {
                    return {};
                }
            }

            if (i + 1 != rotation_sz)
            {
                Matrix ** focused_matrix_arr    = {};
                size_t focused_matrix_vec_sz    = {};

                std::tie(focused_matrix_arr, focused_matrix_vec_sz) = matrix_to_focal(up_to_point_matrix,
                                                                                      i,
                                                                                      focal_suffix_map,
                                                                                      allocator,
                                                                                      err);

                if (*err != SUCCESS)
                {
                    return {};
                }

                Matrix ** up_to_point_incremental_matrix_vec        = std_new_array<std::add_pointer_t<Matrix>>(allocator, focused_matrix_vec_sz);

                for (size_t j = 0u; j < focused_matrix_vec_sz; ++j)
                {
                    Matrix * focused_matrix                     = focused_matrix_arr[j];
                    Matrix ** focal_matrix_vec                  = focal_split_matrix(focused_matrix, focal_sz, allocator, err);

                    if (*err != SUCCESS)
                    {
                        return {};
                    }

                    Matrix ** transformed_focal_vec             = std_new_array<std::add_pointer_t<Matrix>>(allocator, focal_sz);
                    const size_t saved_shape_coeff_arr_offset_0 = shape_coeff_arr_offset;

                    for (size_t focal_idx = 0u; focal_idx < focal_sz; ++focal_idx)
                    {
                        shape_coeff_arr_offset      = saved_shape_coeff_arr_offset_0;
                        Matrix * focal              = focal_matrix_vec[focal_idx];

                        Matrix * transformed_focal  = matrix_transform(focal,
                                                                       focal_sz_vec, focal_sz_vec_offset + 1,
                                                                       focal_suffix_map,
                                                                       rotation_sz_vec, rotation_sz_vec_offset + 1,
                                                                       parameter_bound_ratio_vec, parameter_bound_ratio_vec_offset + 1,
                                                                       base_shape_coeff_sz_container,
                                                                       shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                       dispatch_code_generator,
                                                                       allocator,
                                                                       err,
                                                                       shape_base_promotion_tag,
                                                                       has_logit_unit_reuse_tag,
                                                                       has_logit_group_logit_reuse_tag,
                                                                       has_being_logit_reuse_tag,
                                                                       has_base_matrix_logit_reuse_tag);

                        if (*err != SUCCESS)
                        {
                            return {};
                        }

                        transformed_focal_vec[focal_idx] = transformed_focal;
                    }

                    Matrix * transformed_focused_matrix     = focal_unsplit_matrix(transformed_focal_vec, focal_sz, allocator);
                    up_to_point_incremental_matrix_vec[j]   = transformed_focused_matrix;
                }

                Matrix * incremental_up_to_point_matrix = unfocal_matrix(up_to_point_incremental_matrix_vec, focused_matrix_vec_sz,
                                                                         i,
                                                                         focal_suffix_map,
                                                                         allocator,
                                                                         err);

                if (*err != SUCCESS)
                {
                    return {};
                }

                auto avg_arr                            = std::array<std::add_pointer_t<Matrix>, 2u>{up_to_point_matrix, incremental_up_to_point_matrix};
                Matrix * tmp_result                     = avg(avg_arr.data(), avg_arr.size(), allocator, err);

                if (*err != SUCCESS)
                {
                    return {};
                }

                copy_to(up_to_point_matrix, tmp_result, err);

                if (*err != SUCCESS)
                {
                    return {};
                }
            }
        }

        Matrix * tmp_rs         = series_normalize(incremental_matrix_vec,
                                                   incremental_matrix_vec_sz,
                                                   allocator,
                                                   err);

        if (*err != SUCCESS)
        {
            return {};
        }

        Matrix * final_rs       = feed_forward_transform(tmp_rs,
                                                         base_shape_coeff_sz_container,
                                                         shape_coeff_arr[dispatch_code_generator.get_dispatch_code()], shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                         allocator,
                                                         shape_base_promotion_tag,
                                                         err);

        if (*err != SUCCESS)
        {
            return {};
        }

        copy_to(rs, final_rs, err);

        if (*err != SUCCESS)
        {
            return {};
        }

        return rs;
    }

    template <class MatrixShapeVector, /*inplace_vector<size_t>*/
              class FocalSizeVector, /*inplace_vector<size_t>*/
              class SuffixMap, /*inplace_unordered_map<size_t, inplace_unordered_map<size_t, inplace_vector<inplace_vector<size_t>>?*/
              class RotationSizeVector, /*inplace_vector<size_t>*/
              class ParameterBoundRatioVector, /*inplace_vector<double>*/
              class ShapeBaseCoeffSizeContainer,
              class ShapeBasePromotedFloatType = tensor_std_float_t>
    __device__ constexpr  __attribute__((noinline)) auto matrix_transform_size(MatrixShapeVector matrix_shape_vec,

                                                                               FocalSizeVector focal_sz_vec,
                                                                               SuffixMap focal_suffix_map,

                                                                               RotationSizeVector rotation_sz_vec,
                                                                               ParameterBoundRatioVector parameter_bound_ratio_vec,

                                                                               ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                               size_t hash_table_sz,

                                                                               local_exception_t * err = nullptr, //this is the "new invention" for concurrent error write, last write last win, it's complicated but we now follow a write on error only
                                                                               const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>(),

                                                                               bool has_logit_unit_reuse_tag = true,
                                                                               bool has_logit_group_logit_reuse_tag = true,
                                                                               bool has_being_logit_reuse_tag = true,
                                                                               bool has_base_matrix_logit_reuse_tag = true) -> uint64_t
    {
        using namespace cuda_management::scope_allocator;
        using namespace cuda_management::device_memory;

        local_exception_t local_err         = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        const uint64_t INITIAL_LOGIT_SZ     = uint64_t{1} << 10;
        const uint64_t MULTIPLIER_BASE      = uint64_t{1} << 3;
        const uint64_t EXPONENTIAL_RANGE    = 7u;

        SplitStackAllocator stack_allocator{};

        size_t * shape_arr  = std_new_array<size_t>(stack_allocator, matrix_shape_vec.size());

        for (size_t i = 0u; i < matrix_shape_vec.size(); ++i)
        {
            shape_arr[i] = matrix_shape_vec[i];
        }

        local_exception_t shape_err = check_shape(shape_arr, matrix_shape_vec.size());

        if (shape_err != SUCCESS)
        {
            *err = shape_err;
            return {};
        }

        if (hash_table_sz == 0u)
        {
            *err = OTHER_INVALID_ARGUMENT_CODE;
            return {};
        }

        for (size_t i = 0u; i < EXPONENTIAL_RANGE; ++i)
        {
            scope_guard allocator_grd(&stack_allocator);

            uint64_t tentative_logit_sz                     = INITIAL_LOGIT_SZ * cuda_matrix::utility::unsigned_pow(MULTIPLIER_BASE, i);
            tensor_std_float_t ** tensor_2d_arr             = std_new_array<tensor_std_float_t[]>(stack_allocator, hash_table_sz);

            for (size_t j = 0u; j < hash_table_sz; ++j)
            {
                tensor_2d_arr[j]    = std_new_array<tensor_std_float_t>(stack_allocator, tentative_logit_sz);
            }

            Matrix * tmp_matrix                             = allocate(matrix_shape_vec[0], matrix_shape_vec[1], stack_allocator);
            size_t tensor_arr_sz                            = 0u;
            local_exception_t tmp_err                       = SUCCESS;

            DispatchCodeGenerator dispatch_code_gen(tmp_matrix, hash_table_sz);

            matrix_transform(tmp_matrix,
                             
                             focal_sz_vec, 0u,
                             focal_suffix_map,
                            
                             rotation_sz_vec, 0u,
                             parameter_bound_ratio_vec, 0u,
                             
                             base_shape_coeff_sz_container,
                             tensor_2d_arr, tensor_arr_sz, tentative_logit_sz,

                             dispatch_code_gen,
                             stack_allocator,

                             &tmp_err,
                             shape_base_promotion_tag,  
                             has_logit_unit_reuse_tag,
                             has_logit_group_logit_reuse_tag,
                             has_being_logit_reuse_tag,
                             has_base_matrix_logit_reuse_tag);

            if (tmp_err == SUCCESS)
            {
                return tensor_arr_sz;
            }

            if (tmp_err != INSUFFICIENT_LOGIT_VEC_SIZE_CODE)
            {
                *err = tmp_err;
                return {};
            }
        }

        *err = INSUFFICIENT_LOGIT_VEC_SIZE_CODE;
        return {};        
    }
}

#endif