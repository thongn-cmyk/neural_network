#ifndef __TENSOR_MATRIX_FORWARD_TO_DEVIATION_H__
#define __TENSOR_MATRIX_FORWARD_TO_DEVIATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "tensor_matrix_forward_to_deviation_header.h"
#include <stl_extension/stdx.h>
#include <cuda_management/scope_allocator.>
#include <cuda_maangement/device_memory.h>
#include <matrix/device_tensor/model.h>
#include <matrix/device_tensor/common_operation.h>

namespace taylor_matrix::cuda_matrix::tensor_matrix_forward_to_deviation
{
    #ifdef __CU_ACC__

    //yesterday, I worked on the thesis of the how-tos again

    //it seems that I have been able to quantify what needs to be done and what could be optimized in future optimization efforts

    //first, our rule of thumb is logit density, lower logit size for larger data, achieve better compression (1): this is our true North, what we are trying to achieve

    //second, it is that we must be able to overcome fermat theorem's, by using x = x + f(x) and y = y + x
        //second also means that our projection is projection-complete, there is no such points that our picture can't touch

    //third is that there must be a dynamic programming pattern

    //everything is OK for the NOT base transformation thus far, it is non-negotiatable for the rotation + focus + etc...

    //for the base transformation to work well, we must prove that there exists a solution for every possible projection problems

        //then we'd have to build a tunnel, a narrow fast path to limit the search space "in parallel" with the traditional way
        //this narrow fast path is another attention layer, a recursive continueance of the "scope-in" that we have worked thus far, but on a molecule level

        //we have done well for the sin() * sin() * sin() ...
        //because we know that it is to force the search space to look for lower powers before exploring higher powers, and it does not actually hinder the maximum potential, it is just incredibly difficult to reach the maximum potential

    //in essence, the taylor_projection is FAIR, such is that input variables share equal entropy

    //and the plus operation for the base only works if we assume identity transformation from the first operation AND constructive interference of the projection space
    //the otherwise I can't imagine what it would look like

    //point is that not everything is made equal in this context, such is that we'd still need to "dimensionally reduct" certain variables to build that fast search space path

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
                if (matrix_shape_vec.size() < 2)
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

                Matrix * arg        = taylor_matrix::cuda_matrix::tensor_matrix_operation::allocate(matrix_shape_vec[0],
                                                                                                    matrix_shape_vec[1],
                                                                                                    allocator);
                
                Matrix * expected   = taylor_matrix::cuda_matrix::tensor_matrix_operation::allocate(matrix_shape_vec[0],
                                                                                                    matrix_shape_vec[1],
                                                                                                    allocator);

                taylor_matrix::cuda_amtrix::tensor_matrix_operation::unflatten_to(arg, inp_matrix[offset]);
                taylor_matrix::cuda_matrix::tensor_matrix_operation::unflatten_to(expected, expected_matrix[offset]);

                Matrix * rs         = taylor_matrix::cuda_matrix::tensor_matrix_operation::matrix_transform(arg,
                                                                                                            focal_sz_vec, 0u,
                                                                                                            focal_suffix_map,
                                                                                                            rotation_sz_vec, 0u,
                                                                                                            parameter_bound_ratio_vec,
                                                                                                            utility::to_size_container(base_sz_ic),
                                                                                                            shape_coeff_arr, *shape_coeff_arr_offset, shape_coeff_arr_cap,
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
                    atomicExch(err, OTHER_INVALID_ARGUMENT);
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

                                                                                         cuda_err.get(),
                                                                                         cuda_success_counter.get());

            cudaError_t sync_err = cudaDeviceSynchronize();

            if (sync_err != cudaSuccess)
            {
                throw bad_cuda_synchronization(cudaGetErrorString(sync_err));
            }

            local_exception_t host_err              = cuda_management::host_service::read_cuda_object(cuda_err);
            local_exceptioN_t host_success_counter  = cuda_management::host_service::read_cuda_object(cuda_success_counter);

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