#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_MATRIX_FORWARD_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_MATRIX_FORWARD_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/tensor_model.h>
#include <matrix/device_tensor_model.h>
#include <serializer/dg_buf.h>
#include <vector>
#include <unordered_map>
#include <utility>
#include "local_exception.h"
#include "tensor_matrix_forward_header.h"
#include <cuda_management/scope_allocator.h>
#include <cuda_management/device_memory.h>

namespace taylor_matrix::cuda_matrix::tensor_matrix_forward
{
    //I just dont understand how people would use extern or separate header, .cpp files in modern C++
    //it just seems to me that 99% of the new features involve templates

    #ifdef __CU_ACC__

    // __device__ constexpr __attribute__((noinline)) auto matrix_transform(Matrix * matrix,

    //                                                                  FocalSizeVector focal_sz_vec, size_t focal_sz_vec_offset,
    //                                                                  SuffixMap focal_suffix_map,

    //                                                                  RotationSizeVector rotation_sz_vec, size_t rotation_sz_vec_offset,
    //                                                                  ParameterBoundRatioVector parameter_bound_ratio_vec, size_t parameter_bound_ratio_vec_offset,

    //                                                                  ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
    //                                                                  const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,

    //                                                                  AllocatorInterface&& allocator,

    //                                                                  const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
    //                                                                  bool has_logit_unit_reuse_tag = true,
    //                                                                  bool has_logit_group_logit_reuse_tag = true,
    //                                                                  bool has_being_logit_reuse_tag = true,
    //                                                                  bool has_base_matrix_logit_reuse_tag = true,
    //                                                                  local_exception_t * err = nullptr) -> Matrix *

    __global__ void matrix_transform_helper(tensor_model::tensor_std_float_t ** matrix_arr, size_t matrix_arr_sz,
                                            MatrixShapeVector matrix_shape_vec,

                                            tensor_model::tensor_std_float_t ** output,

                                            FocalSizeVector focal_sz_vec,
                                            SuffixMap focal_suffix_map,
                                            RotationSizeVector rotation_sz_vec,
                                            ParameterBoundRatioVector parameter_bound_ratio_vec,

                                            size_t base_shape_coeff_sz,
                                            const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t * shape_coeff_arr_offset, size_t shape_coeff_arr_cap,

                                            local_exception_t * err)
    {
        using namespace cuda_management::scope_allocator;

        size_t offset = device_get_offset();

        if (offset >= matrix_arr_sz)
        {
            assert(false);
        }

        if (err == nullptr)
        {
            assert(false);
        }

        SplitStackAllocator allocator{};

        {
            scope_guard<SplitStackAllocator> scope_grd(&allocator);

            auto callback_handler = [&]<size_t ShapeBaseCoeffSizeContainerSize>(const std::integral_constant<size_t, ShapeBaseCoeffSizeContainerSize> base_shape_coeff_sz_ic)
            {
                if (matrix_shape_vec.size() != 4u)
                {
                    *err = OTHER_INVALID_ARGUMENT;
                    return;
                }

                size_t local_shape_coeff_arr_offset{};

                if (shape_coeff_arr_offset == nullptr)
                {
                    shape_coeff_arr_offset = &local_shape_coeff_arr_offset;
                }

                *shape_coeff_arr_offset = 0u;

                Matrix * arg    = taylor_matrix::cuda_matrix::tensor_matrix_operation::allocate(matrix_shape_vec[0], matrix_shape_vec[1], allocator);
                Matrix * rs     = taylor_matrix::cuda_matrix::tensor_matrix_operation::matrix_transform(arg,
                                                                                                        focal_sz_vec, 0u,
                                                                                                        focal_suffix_map,
                                                                                                        rotation_sz_vec, 0u,
                                                                                                        parameter_bound_ratio_vec, 0u,
                                                                                                        base_shape_coeff_sz_ic,
                                                                                                        shape_coeff_arr, *shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                                        allocator,
                                                                                                        err);

                if (*err != SUCCESS)
                {
                    return;
                }

                taylor_matrix::cuda_matrix::tensor_matrix_operation::flatten_to(output[offset], rs);
            };

            to_constant_number(base_shape_coeff_sz,
                               std::integral_constant<size_t, MIN_BASE_SHAPE_COEFF_SZ>{},
                               std::integral_constant<size_t, MAX_BASE_SHAPE_COEFF_SZ>{},
                               callback_handler,
                               err);
        }
    }

    #endif

    extern void matrix_transform(tensor_model::tensor_std_float_t ** matrix_arr, size_t matrix_arr_sz,
                                 MatrixShapeVector matrix_shape_vec,

                                 tensor_model::tensor_std_float_t ** output,

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

            matrix_transform_helper<<<blk_per_grd_sz, thread_per_blk_sz>>>(matrix_arr, matrix_arr_sz,
                                                                           matrix_shape_vec,

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