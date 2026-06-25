//__GIT_INTEGRATION_TAG__

#ifndef __TENSOR_MATRIX_FORWARD_TO_DEVIATION_H__
#define __TENSOR_MATRIX_FORWARD_TO_DEVIATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "tensor_matrix_forward_to_deviation_header.h"
#include <stl_extension/stdx.h>

#ifdef __CUDACC__

#include <cuda_management/scope_allocator.h>
#include <cuda_management/device_memory.h>
#include <matrix/device_tensor/model.h>
#include <matrix/device_tensor/common_operation.h>
#include "tensor_matrix_operation.h"
#include <deviation_projector/cuda_device/local_exception.h>
#include <deviation_projector/cuda_device/cuda_device.h>
#include "utility.h"
#include <cuda_management/kernel_dispatch.h>
#include "dispatch_code_generator.h"

#endif

#include <general_definition/float_def.h>
#include "local_exception.h"

namespace taylor_matrix::cuda_matrix::tensor_matrix_forward_to_deviation
{
    using namespace float_def;

    #ifdef __CUDACC__

    using DispatchCodeGenerator = taylor_matrix::cuda_matrix::dispatch_code_generator::DispatchCodeGenerator;

    __device__ static constexpr inline size_t MIN_BASE_SHAPE_COEFF_SZ   = 1u;
    __device__ static constexpr inline size_t MAX_BASE_SHAPE_COEFF_SZ   = 6u;

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
                                                         const std::add_pointer_t<tensor_model::tensor_std_float_t> * shape_coeff_arr, size_t * shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                         
                                                         size_t hash_table_sz,

                                                         local_exception_t * err,
                                                         uint32_t * success_launch_counter)
    {
        using namespace cuda_management::scope_allocator;
        using namespace cuda_management::device_memory;
        using namespace device_tensor::model;
        using namespace local_exception;

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
            scope_guard scope_grd(&allocator);

            auto callback_handler = [&]<size_t BaseSize>(const std::integral_constant<size_t, BaseSize> base_sz_ic)
            {
                size_t * shape_arr  = std_new_array<size_t>(allocator, matrix_shape_vec.size());

                for (size_t i = 0u; i < matrix_shape_vec.size(); ++i)
                {
                    shape_arr[i] = matrix_shape_vec[i];
                }

                local_exception_t shape_err = taylor_matrix::cuda_matrix::tensor_matrix_operation::check_shape(shape_arr, matrix_shape_vec.size());

                if (shape_err != SUCCESS)
                {
                    atomicExch(err, shape_err);
                    return;
                }

                if (hash_table_sz == 0u)
                {
                    atomicExch(err, OTHER_INVALID_ARGUMENT_CODE);
                    return;
                }

                if (matrix_shape_vec.size() < 2u)
                {
                    assert(false);
                }

                size_t local_shape_coeff_arr_offset{};

                if (shape_coeff_arr_offset == nullptr)
                {
                    shape_coeff_arr_offset = &local_shape_coeff_arr_offset;
                }

                *shape_coeff_arr_offset = 0u;

                Matrix * arg        = taylor_matrix::cuda_matrix::tensor_matrix_operation::allocate(matrix_shape_vec[0],
                                                                                                    matrix_shape_vec[1],
                                                                                                    allocator);

                Matrix * expected   = taylor_matrix::cuda_matrix::tensor_matrix_operation::allocate(matrix_shape_vec[0],
                                                                                                    matrix_shape_vec[1],
                                                                                                    allocator);

                taylor_matrix::cuda_matrix::tensor_matrix_operation::unflatten_to(arg, inp_matrix[offset]);
                taylor_matrix::cuda_matrix::tensor_matrix_operation::unflatten_to(expected, expected_matrix[offset]);
                DispatchCodeGenerator dispatch_code_generator(arg, hash_table_sz);

                Matrix * rs         = taylor_matrix::cuda_matrix::tensor_matrix_operation::matrix_transform(arg,
                                                                                                            focal_sz_vec, 0u,
                                                                                                            focal_suffix_map,
                                                                                                            rotation_sz_vec, 0u,
                                                                                                            parameter_bound_ratio_vec, 0u,
                                                                                                            utility::to_size_container(base_sz_ic),
                                                                                                            shape_coeff_arr, *shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                                            dispatch_code_generator,
                                                                                                            allocator,
                                                                                                            &local_err);

                if (local_err != SUCCESS)
                {
                    atomicExch(err, local_err);
                    return;
                }

                deviation_projector::cuda_device::local_exception::local_exception_t deviation_err = deviation_projector::cuda_device::local_exception::SUCCESS;

                double deviation    = deviation_projector::cuda_device::get_deviation(deviation_calculator_device,
                                                                                      rs,
                                                                                      expected,
                                                                                      &deviation_err);

                if (deviation_err != deviation_projector::cuda_device::local_exception::SUCCESS)
                {
                    atomicExch(err, OTHER_INVALID_ARGUMENT_CODE);
                    return;
                }

                atomicAdd(output, deviation);
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

    //I think that matrix_arr_sz should be compromised at this site, I'm unsure
    //this is runtime-deterministic at the function

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
                                              const std::add_pointer_t<tensor_model::tensor_std_float_t> * shape_coeff_arr, size_t * shape_coeff_arr_offset, size_t shape_coeff_arr_cap,

                                              size_t hash_table_sz)
    {
        #ifdef __CUDACC__
        {
            size_t blk_per_grid_sz{};
            size_t thread_per_blk_sz{};

            std::shared_ptr<local_exception_t> cuda_err     = cuda_management::host_service::make_cuda_object<local_exception_t>(SUCCESS);
            std::shared_ptr<uint32_t> cuda_success_counter  = cuda_management::host_service::make_cuda_object<uint32_t>(0u);

            std::tie(blk_per_grid_sz, thread_per_blk_sz)    = cuda_management::kernel_dispatch::get_block_thread(matrix_arr_sz);

            stdx::smp_guard smp_grd(cuda_management::kernel_dispatch::get_semaphore());

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

                                                                                         hash_table_sz,

                                                                                         cuda_err.get(),
                                                                                         cuda_success_counter.get());

            cudaError_t sync_err = cudaDeviceSynchronize();

            if (sync_err != cudaSuccess)
            {
                throw bad_cuda_synchronization(cudaGetErrorString(sync_err));
            }

            local_exception_t host_err              = cuda_management::host_service::read_cuda_object(cuda_err);
            uint32_t host_success_counter           = cuda_management::host_service::read_cuda_object(cuda_success_counter);

            throw_error_code(host_err);

            if (host_success_counter != matrix_arr_sz)
            {
                throw other_runtime_error("bad cuda launch, success head count mismatched");
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