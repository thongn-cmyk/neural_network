#ifndef __TENSOR_MATRIX_FORWARD_TO_DEVIATION_EXTERN_H__
#define __TENSOR_MATRIX_FORWARD_TO_DEVIATION_EXTERN_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/tensor_model.h>
#include <serializer/dg_buf.h>
#include <vector>
#include <unordered_map>
#include <utility>
#include <general_definition/float_def.h>

namespace taylor_matrix::cuda_matrix
{
    using namespace float_def;

    using FocalSizeVector   = decltype(dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(std::declval<const std::vector<size_t>&>(),
                                                                                       std::declval<std::string&>()));

    using SuffixMap         = decltype(dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(std::declval<const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>&>(),
                                                                                       std::declval<std::string&>()));

    extern void matrix_transform_to_deviation(tensor_model::tensor_std_float_t ** inp_matrix,
                                              tensor_model::tensor_std_float_t ** expected_matrix, size_t matrix_arr_sz,

                                              size_t * matrix_shape, size_t matrix_shape_sz,

                                              uint8_t deviation_calculator_device, 

                                              mdc_float_t * output,

                                              FocalSizeVector focal_sz_vec,
                                              SuffixMap focal_suffix_map,

                                              size_t base_shape_coeff_sz,
                                              const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t * shape_coeff_arr_offset, size_t shape_coeff_arr_cap,

                                              exception_t * err);
}

#endif