//HEADER_CONTROL 3

#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_PROCESS_UNIT_OPERATION_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_PROCESS_UNIT_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include "tensor_model.h"
#include <stdexcept>
#include "shape_projection.h"
#include <array>
#include <algorithm>
#include <cstring>
#include "local_exception.h"

namespace taylor_matrix::cuda_matrix::tensor_process_unit_operation
{
    using namespace taylor_matrix::cuda_matrix::tensor_model;
    using namespace taylor_matrix::cuda_matrix::utility;
    using namespace taylor_matrix::cuda_matrix::local_exception;

    //--CREATE-BY-DEFAULT--
    
    //--READ--

    __device__ constexpr auto flatten_to(tensor_std_float_t * dst, const ProcessUnit& src) -> tensor_std_float_t *
    {
        std::memcpy(dst,
                    src.logit_vec.data(),
                    src.logit_vec.size() * sizeof(tensor_std_float_t));

        return next(dst, src.logit_vec.size());
    }

    __device__ constexpr auto flatten_size(const ProcessUnit& src) -> size_t
    {
        return PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;
    }

    //--OPERATION--

    __device__ constexpr auto empty_as(const tensor_std_float_t&) -> tensor_std_float_t
    {
        return 0;
    }

    __device__ constexpr auto empty_as(const ProcessUnit&) -> ProcessUnit
    {
        ProcessUnit rs{};
        fill(rs.logit_vec.begin(), rs.logit_vec.end(), 0);

        return rs;
    }

    __device__ constexpr auto unflatten_to(ProcessUnit& dst, const tensor_std_float_t * src) -> const tensor_std_float_t *
    {
        std::memcpy(dst.logit_vec.data(),
                    src,
                    dst.logit_vec.size() * sizeof(tensor_std_float_t));

        return next(src, dst.logit_vec.size());
    }

    template <class ShapeBaseCoeffSizeContainer, class ShapeBasePromotedFloatType = tensor_model::tensor_std_float_t>
    __device__ constexpr __attribute__((noinline)) auto two_to_one_project(const ProcessUnit& lhs,
                                                                           const ProcessUnit& rhs,
                                                                           ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                           const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                           local_exception_t * err = nullptr,
                                                                           const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                           bool has_logit_reuse_tag = true) -> ProcessUnit
    {
        constexpr size_t COMBINED_DIMENSION_SZ = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * 2u;

        std::array<tensor_model::tensor_std_float_t, COMBINED_DIMENSION_SZ> combined{};

        copy(lhs.logit_vec.begin(), lhs.logit_vec.end(), combined.data());
        copy(rhs.logit_vec.begin(), rhs.logit_vec.end(), next(combined.data(), lhs.logit_vec.size()));

        std::array<tensor_model::tensor_std_float_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ> child{};

        shape_projection::multidimensional_taylor_shape_project(combined.data(), to_size_container(std::integral_constant<size_t, COMBINED_DIMENSION_SZ>{}),
                                                                base_shape_coeff_sz_container,
                                                                shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                child.data(), child.size(),
                                                                err,
                                                                shape_base_promotion_tag,
                                                                has_logit_reuse_tag);

        return {.logit_vec = child};
    }

    template <class ShapeBaseCoeffSizeContainer, size_t BATCH_SZ, class ShapeBasePromotedFloatType = tensor_model::tensor_std_float_t>
    __device__ constexpr __attribute__((noinline)) auto batch_two_to_one_project(const ProcessUnit * lhs_arr,
                                                                                 const ProcessUnit * rhs_arr,
                                                                                 const std::integral_constant<size_t, BATCH_SZ> batch_sz,
                                                                                 ProcessUnit * out_arr,
                                                                                 ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                                 const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                                 local_exception_t * err = nullptr,
                                                                                 const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                                 bool has_logit_reuse_tag = true)
    {
        static_assert(BATCH_SZ != 0u);

        constexpr size_t COMBINED_DIMENSION_SZ  = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * 2u;
        constexpr size_t INPUT_ARRAY_SZ         = COMBINED_DIMENSION_SZ * BATCH_SZ;
        constexpr size_t OUTPUT_ARRAY_SZ        = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * BATCH_SZ;

        tensor_model::tensor_std_float_t input_arr[INPUT_ARRAY_SZ];
        ShapeBasePromotedFloatType shape_output_arr[OUTPUT_ARRAY_SZ];

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

        shape_projection::batch_multidimensional_taylor_shape_project(input_arr, to_size_container(std::integral_constant<size_t, COMBINED_DIMENSION_SZ>{}), to_size_container(batch_sz),
                                                                      base_shape_coeff_sz_container,
                                                                      shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                      shape_output_arr, std::integral_constant<size_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ>{},
                                                                      err,
                                                                      has_logit_reuse_tag);

        if (err != nullptr && *err != SUCCESS)
        {
            return;
        }

        for (size_t i = 0u; i < BATCH_SZ; ++i)
        {
            out_arr[i] = ProcessUnit{};

            for (size_t j = 0u; j < PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ; ++j)
            {
                size_t col_idx  = i;
                size_t row_idx  = j;
                size_t idx      = row_idx * BATCH_SZ + col_idx;

                out_arr[i].logit_vec[j] = shape_output_arr[idx];   
            }
        }
    }

    __device__ constexpr auto deparameterize(const ProcessUnit& process_unit,
                                             double perc) -> ProcessUnit
    {
        size_t tentative_deparam_sz     = process_unit.logit_vec.size() * perc;
        size_t deparam_sz               = clamp(tentative_deparam_sz, size_t{0u}, process_unit.logit_vec.size());
        size_t active_sz                = process_unit.logit_vec.size() - deparam_sz;

        std::array<tensor_std_float_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ> rs{};

        for (size_t i = 0u; i < active_sz; ++i)
        {
            rs[i] = process_unit.logit_vec[i];
        }

        for (size_t i = 0u; i < deparam_sz; ++i)
        {
            rs[active_sz + i] = 0;
        }

        return {.logit_vec = rs};
    }

    __device__ constexpr auto accumulate(const ProcessUnit& lhs,
                                         const ProcessUnit& rhs) -> ProcessUnit
    {
        ProcessUnit rs{};

        for (size_t i = 0u; i < lhs.logit_vec.size(); ++i)
        {
            rs.logit_vec[i] = lhs.logit_vec[i] + rhs.logit_vec[i];
        }

        return rs;
    }

    __device__ constexpr auto accumulate(const ProcessUnit * process_vec,
                                         size_t process_vec_sz,
                                         local_exception_t * err = nullptr) -> ProcessUnit
    {
        if (process_vec_sz == 0u) [[unlikely]]
        {
            if (err != nullptr)
            {
                *err = OTHER_INVALID_ARGUMENT_CODE;
            }

            return {};
        }

        ProcessUnit result = process_vec[0];

        for (size_t i = 1u; i < process_vec_sz; ++i)
        {
            result = accumulate(result, process_vec[i]);
        }

        return result;
    }

    template <class ValueType>
    __device__ constexpr auto div(const ProcessUnit& process_unit,
                                  const ValueType& value) -> ProcessUnit
    {
        ProcessUnit rs{};

        for (size_t i = 0u; i < rs.logit_vec.size(); ++i)
        {
            rs.logit_vec[i] = process_unit.logit_vec[i] / value;
        }

        return rs;
    }

    __device__ constexpr auto avg(const ProcessUnit * process_vec,
                                  size_t process_vec_sz,
                                  local_exception_t * err = nullptr) -> ProcessUnit
    {
        if (process_vec_sz == 0u)
        {
            if (err != nullptr)
            {
                *err = OTHER_INVALID_ARGUMENT_CODE;
            }

            return {};
        }

        return div(accumulate(process_vec, process_vec_sz, err), process_vec_sz);
    }
}

#endif