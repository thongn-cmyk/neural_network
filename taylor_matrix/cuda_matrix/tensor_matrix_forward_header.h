#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_MATRIX_FORWARD_HEADER_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_MATRIX_FORWARD_HEADER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/tensor_model.h>
#include <serializer/dg_buf.h> 
#include <vector>
#include <unordered_map>
#include <utility>
#include "local_exception.h"

namespace taylor_matrix::cuda_matrix::tensor_matrix_forward
{
    using namespace taylor_matrix::cuda_matrix::local_exception;

    using MatrixShapeVector         = decltype(dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(std::declval<const std::vector<size_t>&>(),
                                                                                               std::declval<std::string&>()));

    using FocalSizeVector           = decltype(dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(std::declval<const std::vector<size_t>&>(),
                                                                                               std::declval<std::string&>())); 

    using SuffixMap                 = decltype(dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(std::declval<const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>&>(),
                                                                                               std::declval<std::string&>()));

    using RotationSizeVector        = decltype(dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(std::declval<const std::vector<size_t>&>(),
                                                                                               std::declval<std::string&>()));

    using ParameterBoundRatioVector = decltype(dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(std::declval<const std::vector<double>&>(),
                                                                                               std::declval<std::string&>()));

    extern void matrix_transform(tensor_model::tensor_std_float_t ** matrix_arr, size_t matrix_arr_sz,
                                 MatrixShapeVector matrix_shape_vec,

                                 tensor_model::tensor_std_float_t ** output,

                                 FocalSizeVector focal_sz_vec,
                                 SuffixMap focal_suffix_map,
                                 RotationSizeVector rotation_sz_vec,
                                 ParameterBoundRatioVector parameter_bound_ratio_vec,

                                 size_t base_shape_coeff_sz,
                                 const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t * shape_coeff_arr_offset, size_t shape_coeff_arr_cap,

                                 local_exception_t * device_mem_err);
}

#endif