#ifndef __TENSOR_MATRIX_FORWARD_TO_DEVIATION_H__
#define __TENSOR_MATRIX_FORWARD_TO_DEVIATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "tensor_matrix_forward_to_deviation_header.h"
#include <stl_extension/stdx.h>

namespace taylor_matrix::cuda_matrix::tensor_matrix_forward_to_deviation
{
    #ifdef __CU_ACC__

    __global__ void matrix_transform_to_deviation_helper(tensor_model::tensor_std_float_t ** inp_matrix,
                                                         tensor_model::tensor_std_float_t ** expected_matrix, size_t matrix_arr_sz,

                                                         MatrixShapeVector matrix_shape_vec,

                                                         uint8_t deviation_calculator_device,
                                                         mdc_float_t * output,

                                                         FocalSizeVector focal_sz_vec,
                                                         SuffixMap focal_suffix_map,
                                                         RotationSizeVector rotation_sz_vec,
                                                         ParameterBoundRatioVector parameter_bound_ratio_vec,

                                                         size_t base_shape_coeff_sz,
                                                         const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t * shape_coeff_arr_offset, size_t shape_coeff_arr_cap,

                                                         local_exception_t * err)
    {
        size_t offset = device_get_offset();

        if (offset >= matrix_arr_sz)
        {
            assert(false);
        }

    }

    #endif

    extern void matrix_transform_to_deviation(tensor_model::tensor_std_float_t ** inp_matrix,
                                              tensor_model::tensor_std_float_t ** expected_matrix, size_t matrix_arr_sz,

                                              MatrixShapeVector matrix_shape_vec,

                                              uint8_t deviation_calculator_device,
                                              mdc_float_t * output,

                                              FocalSizeVector focal_sz_vec,
                                              SuffixMap focal_suffix_map,
                                              RotationSizeVector rotation_sz_vec,
                                              ParameterBoundRatioVector parameter_bound_ratio_vec,

                                              size_t base_shape_coeff_sz,
                                              const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t * shape_coeff_arr_offset, size_t shape_coeff_arr_cap,

                                              local_exception_t * device_mem_err)
    {
        #ifdef __CU_ACC__
        {
            size_t blk_per_grid_sz{};
            size_t thread_per_blk_sz{};

            std::tie(blk_per_grid_sz, thread_per_blk_sz) = cuda_management::kernel_dispatch::get_block_thread(matrix_arr_sz);

            stdx::smp_guard<std::semaphore> smp_grd(cuda_management::kernel_dispatch::get_semaphore());

            matrix_transform_to_deviation_helper<<<blk_per_grid_sz, thread_per_blk_sz>>>(inp_matrix,
                                                                                         expected_matrix, matrix_arr_sz,

                                                                                         matrix_shape_vec,

                                                                                         deviation_calculator_device,
                                                                                         output,

                                                                                         focal_sz_vec,
                                                                                         focal_suffix_map,
                                                                                         rotation_sz_vec,
                                                                                         parameter_bound_ratio_vec,

                                                                                         base_shape_coeff_sz,
                                                                                         shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,

                                                                                         device_mem_err);

            cudaError_t sync_err = cudaDeviceSynchronize();

            if (sync_err != cudaSuccess)
            {
                throw bad_cuda_synchronization{};
            }
        }
        #else
        {
            throw cuda_device_not_supported{};
        }
        #endif

    }
}

#endif