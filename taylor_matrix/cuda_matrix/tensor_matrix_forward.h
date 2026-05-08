#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_MATRIX_FORWARD_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_TENSOR_MATRIX_FORWARD_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/tensor_model.h>
#include <matrix/device_tensor/model.h>
#include <serializer/dg_buf.h>
#include <vector>
#include <unordered_map>
#include <utility>
#include "local_exception.h"
#include "tensor_matrix_forward_header.h"
#include <cuda_management/scope_allocator.h>
#include <cuda_management/device_memory.h>
#include <cuda_management/host_service_header.h>

namespace taylor_matrix::cuda_matrix::tensor_matrix_forward
{
    //I just dont understand how people would use extern or separate header, .cpp files in modern C++
    //it just seems to me that 99% of the new features involve templates

    #ifdef __CU_ACC__

    __global__ void matrix_transform_helper(tensor_model::tensor_std_float_t ** matrix_arr, size_t matrix_arr_sz,
                                            MatrixShapeVector matrix_shape_vec,

                                            tensor_model::tensor_std_float_t ** output,

                                            FocalSizeVector focal_sz_vec,
                                            SuffixMap focal_suffix_map,
                                            RotationSizeVector rotation_sz_vec,
                                            ParameterBoundRatioVector parameter_bound_ratio_vec,

                                            size_t base_shape_coeff_sz,
                                            const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t * shape_coeff_arr_offset, size_t shape_coeff_arr_cap,

                                            local_exception_t * err,
                                            size_t * success_launch_counter)
    {
        using namespace cuda_management::scope_allocator;
        using namespace device_tensor::model;

        size_t offset = blockIdx.x * blockDim.x + threadIdx.x;

        if (offset >= matrix_arr_sz)
        {
            return;
        }

        if (err == nullptr)
        {
            assert(false);
        }

        if (success_launch_counter == nullptr)
        {
            assert(false);
        }

        SplitStackAllocator allocator{};
        local_exception_t local_err = SUCCESS;

        {
            scope_guard<SplitStackAllocator> scope_grd(&allocator);

            auto callback_handler = [&]<size_t BaseSize>(const std::integral_constant<size_t, BaseSize> base_sz_ic)
            {
                if (matrix_shape_vec.size() < 2u)
                {
                    atomicExch(err, OTHER_INVALID_ARGUMENT);
                    return;
                }

                size_t local_shape_coeff_arr_offset{};

                if (shape_coeff_arr_offset == nullptr)
                {
                    shape_coeff_arr_offset = &local_shape_coeff_arr_offset;
                }

                *shape_coeff_arr_offset = 0u;

                Matrix * arg    = taylor_matrix::cuda_matrix::tensor_matrix_operation::allocate(matrix_shape_vec[0],
                                                                                                matrix_shape_vec[1],
                                                                                                allocator);

                taylor_matrix::cuda_matrix::tensor_matrix_operation::unflatten_to(arg, matrix_arr[offset]);
                
                Matrix * rs     = taylor_matrix::cuda_matrix::tensor_matrix_operation::matrix_transform(arg,
                                                                                                        focal_sz_vec, 0u,
                                                                                                        focal_suffix_map,
                                                                                                        rotation_sz_vec, 0u,
                                                                                                        parameter_bound_ratio_vec, 0u,
                                                                                                        utility::to_size_container(base_sz_ic),
                                                                                                        shape_coeff_arr, *shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                                        allocator,
                                                                                                        &local_err);

                if (local_err != SUCCESS)
                {
                    atomicExch(err, local_err);
                    return;
                }

                taylor_matrix::cuda_matrix::tensor_matrix_operation::flatten_to(output[offset], rs);
            };

            utility::to_constant_number(base_shape_coeff_sz,
                                        std::integral_constant<size_t, MIN_BASE_SHAPE_COEFF_SZ>{},
                                        std::integral_constant<size_t, MAX_BASE_SHAPE_COEFF_SZ>{},
                                        callback_handler,
                                        &local_err);

            if (local_err != SUCCESS)
            {
                atomicExch(err, local_err);
                return;
            }
        }

        atomicAdd(success_launch_counter, 1u);
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
                                 const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t * shape_coeff_arr_offset, size_t shape_coeff_arr_cap)
    {
        #ifdef __CU_ACC__
        {
            size_t blk_per_grid_sz{};
            size_t thread_per_blk_sz{};

            std::shared_ptr<local_exception_t> cuda_err     = cuda_management::host_service::make_cuda_object<local_exception_t>(SUCCESS);
            std::shared_ptr<size_t> cuda_success_counter    = cuda_management::host_service::make_cuda_object<size_t>(0u);

            std::tie(blk_per_grid_sz, thread_per_blk_sz)    = cuda_management::kernel_dispatch::get_block_thread(matrix_arr_sz);

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

                                                                           cuda_err.get(),
                                                                           cuda_success_counter.get());

            cudaError_t sync_err = cudaDeviceSynchronize();

            if (sync_err != cudaSuccess)
            {
                throw bad_cuda_synchronization(cudaGetErrorString(sync_err));
            }

            local_exception_t host_err  = cuda_management::host_service::read_cuda_object(cuda_err);
            size_t host_success_counter = cuda_management::host_service::read_cuda_object(cuda_success_counter);

            throw_error_code(host_err);

            if (host_success_counter != matrix_arr_sz)
            {
                throw other_runtime_error("bad cuda launch, success head count mismatched");
            }
        }
        #else
        {
            throw cuda_device_not_supported();
        }
        #endif
    }
}

#endif