#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <matrix/tensor_model.h>
#include <serializer/dg_buf.h>
#include <vector>
#include <unordered_map>
#include <utility>
#include "local_exception.h"
#include "local_host_exception.h"
#include "tensor_matrix_forward.h"

#ifdef __CUDACC__

#include <matrix/device_tensor/model.h>
#include <cuda_management/scope_allocator.h>
#include <cuda_management/device_memory.h>
#include <cuda_management/host_service.h>
#include "utility.h"
#include "tensor_matrix_operation.h"
#include <cuda_management/kernel_dispatch.h>
#include "dispatch_code_generator.h"

#endif

namespace taylor_matrix::cuda_matrix::tensor_matrix_forward
{
    #ifdef __CUDACC__

    __device__ static constexpr inline size_t MIN_BASE_SHAPE_COEFF_SZ   = 1u;
    __device__ static constexpr inline size_t MAX_BASE_SHAPE_COEFF_SZ   = 6u;

    __global__ void matrix_transform_helper(tensor_model::tensor_std_float_t ** matrix_arr, size_t matrix_arr_sz,
                                            MatrixShapeVector matrix_shape_vec,

                                            tensor_model::tensor_std_float_t ** output,

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

        using DispatchCodeGenerator = taylor_matrix::cuda_matrix::dispatch_code_generator::DispatchCodeGenerator;

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
                
                size_t local_shape_coeff_arr_offset = 0u;

                if (shape_coeff_arr_offset == nullptr)
                {
                    shape_coeff_arr_offset = &local_shape_coeff_arr_offset;
                }

                Matrix * arg    = taylor_matrix::cuda_matrix::tensor_matrix_operation::allocate(matrix_shape_vec[0],
                                                                                                matrix_shape_vec[1],
                                                                                                allocator);

                taylor_matrix::cuda_matrix::tensor_matrix_operation::unflatten_to(arg, matrix_arr[offset]);
                DispatchCodeGenerator dispatch_code_generator(arg, hash_table_sz);

                Matrix * rs     = taylor_matrix::cuda_matrix::tensor_matrix_operation::matrix_transform(arg,
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

        atomicAdd(success_launch_counter, uint32_t{1});
    }

    __global__ void matrix_transform_size_helper(MatrixShapeVector matrix_shape_vec,
                                                 FocalSizeVector focal_sz_vec,
                                                 SuffixMap focal_suffix_map,
                                                 RotationSizeVector rotation_sz_vec,
                                                 ParameterBoundRatioVector parameter_bound_ratio_vec,

                                                 size_t base_shape_coeff_sz,
                                                 size_t hash_table_sz,

                                                 local_exception_t * err,
                                                 size_t * result,
                                                 bool * kernel_call_flag)
    {
        if (err == nullptr)
        {
            assert(false);
        }

        if (result == nullptr)
        {
            assert(false);
        }

        if (kernel_call_flag == nullptr)
        {
            assert(false);
        }

        auto callback_handler = [&]<size_t BaseSize>(const std::integral_constant<size_t, BaseSize> base_sz_ic)
        {
            *result = taylor_matrix::cuda_matrix::tensor_matrix_operation::matrix_transform_size(matrix_shape_vec,
                                                                                                 focal_sz_vec,
                                                                                                 focal_suffix_map,
                                                                                                 rotation_sz_vec,
                                                                                                 parameter_bound_ratio_vec,
                                                                                                 utility::to_size_container(base_sz_ic),
                                                                                                 hash_table_sz,
                                                                                                 err);
        };

        utility::to_constant_number(base_shape_coeff_sz,
                                    std::integral_constant<size_t, MIN_BASE_SHAPE_COEFF_SZ>{},
                                    std::integral_constant<size_t, MAX_BASE_SHAPE_COEFF_SZ>{},
                                    callback_handler,
                                    err);

        *kernel_call_flag = true;
    }

    #endif

    //I think that matrix_arr_sz should be compromised at this site, I'm unsure
    //this is runtime-deterministic at the function

    extern void matrix_transform(tensor_model::tensor_std_float_t ** matrix_arr, size_t matrix_arr_sz,
                                 MatrixShapeVector matrix_shape_vec,

                                 tensor_model::tensor_std_float_t ** output,

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
            using namespace local_exception;

            size_t blk_per_grid_sz{};
            size_t thread_per_blk_sz{};

            std::shared_ptr<local_exception_t> cuda_err     = cuda_management::host_service::make_cuda_object<local_exception_t>(SUCCESS);
            std::shared_ptr<uint32_t> cuda_success_counter  = cuda_management::host_service::make_cuda_object<uint32_t>(0u);
            std::tie(blk_per_grid_sz, thread_per_blk_sz)    = cuda_management::kernel_dispatch::get_block_thread(matrix_transform_helper,
                                                                                                                 matrix_arr_sz);
            // blk_per_grid_sz                                 = 1;
            // thread_per_blk_sz                               = matrix_arr_sz;

            stdx::smp_guard smp_grd(cuda_management::kernel_dispatch::get_semaphore());

            matrix_transform_helper<<<blk_per_grid_sz, thread_per_blk_sz>>>(matrix_arr, matrix_arr_sz,
                                                                            matrix_shape_vec,

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

            local_exception_t host_err      = cuda_management::host_service::read_cuda_object(cuda_err);
            uint32_t host_success_counter   = cuda_management::host_service::read_cuda_object(cuda_success_counter);

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

    extern auto matrix_transform_size(MatrixShapeVector matrix_shape_vec,
                                      FocalSizeVector focal_sz_vec,
                                      SuffixMap focal_suffix_map,
                                      RotationSizeVector rotation_sz_vec,
                                      ParameterBoundRatioVector parameter_bound_ratio_vec,
                                      size_t base_shape_coeff_sz,
                                      size_t hash_table_sz) -> uint64_t
    {
        #ifdef __CUDACC__
        {
            using namespace local_exception;

            std::shared_ptr<local_exception_t> cuda_err     = cuda_management::host_service::make_cuda_object<local_exception_t>(SUCCESS);
            std::shared_ptr<size_t> cuda_result             = cuda_management::host_service::make_cuda_object<size_t>();
            std::shared_ptr<bool> cuda_kernel_call_flag     = cuda_management::host_service::make_cuda_object<bool>(false);

            stdx::smp_guard smp_grd(cuda_management::kernel_dispatch::get_semaphore());

            matrix_transform_size_helper<<<1, 1>>>(matrix_shape_vec,
                                                   focal_sz_vec,
                                                   focal_suffix_map,
                                                   rotation_sz_vec,
                                                   parameter_bound_ratio_vec,
                                                   base_shape_coeff_sz,
                                                   hash_table_sz,
                                                   cuda_err.get(),
                                                   cuda_result.get(),
                                                   cuda_kernel_call_flag.get());

            cudaError_t sync_err = cudaDeviceSynchronize();

            if (sync_err != cudaSuccess)
            {
                throw bad_cuda_synchronization(cudaGetErrorString(sync_err));
            }

            bool host_kernel_call_flag  = cuda_management::host_service::read_cuda_object(cuda_kernel_call_flag);

            if (host_kernel_call_flag == false)
            {
                throw other_runtime_error("kernel call failed");
            }

            local_exception_t host_err  = cuda_management::host_service::read_cuda_object(cuda_err);
            size_t host_result          = cuda_management::host_service::read_cuda_object(cuda_result);

            throw_error_code(host_err);

            return host_result;
        }
        #else
        {
            throw cuda_device_not_supported();
        }
        #endif
    }
}