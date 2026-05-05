#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_H__

#include <memory_management/cu_immutable_memory.h>
#include <serializer/dg_buf.h>
#include <matrix/the_matrix_interface.h>
#include <vector>
#include <unordered_map>
#include <general_definition/float_def.h>
#include <mutex_extension/fair_mutex.h>
#include <matrix/tensor_model.h>
#include <vector>
#include <unordered_map>
#include "tensor_matrix_forward_header.h"
#include <cuda_management/host_service_header.h>
#include <funnel/funnel.h>

namespace taylor_matrix::cuda_matrix::the_cuda_matrix
{
    using namespace the_matrix;
    using namespace taylor_matrix::cuda_matrix::tensor_matrix_forward;

    using tensor_std_float_t                = tensor_model::tensor_std_float_t;
    using cuda_matrix_kernel_exception_t    = uint8_t;

    class TheCudaMatrix: public virtual MatrixInterface
    {
        private:
            
            struct CudaResource
            {
                std::shared_ptr<MatrixShapeVector> matrix_shape_vec;
                std::shared_ptr<FocalSizeVector> focal_sz_vec;
                std::shared_ptr<SuffixMap> focal_suffix_map;
                std::shared_ptr<RotationSizeVector> rotation_sz_vec;
                std::shared_ptr<ParameterBoundRatioVector> parameter_bound_ratio_vec;

                std::shared_ptr<tensor_model::tensor_std_float_t[]> logit_cuda_arr;
            };

            struct CudaDispatchables
            {
                tensor_model::tensor_std_float_t ** inp_cuda_matrix_arr;
                tensor_model::tensor_std_float_t ** out_cuda_matrix_arr;
            };

            struct CudaDispatchable
            {
                tensor_model::tensor_std_float_t * inp_cuda_matrix;
                tensor_model::tensor_std_float_t * out_cuda_matrix;
            };

            std::vector<size_t> shape_vec;
            std::vector<size_t> focal_sz_vec;
            std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map;
            std::vector<size_t> rotation_sz_vec;
            std::vector<double> parameter_bound_ratio_vec;

            std::vector<tensor_std_float_t> shape_coeff_vec;
            size_t base_shape_coeff_sz;

            std::optional<size_t> deviation_operation_window;
            std::optional<CudaResource> cuda_resource;
            bool is_set_update_available;

            cuda_management::host_service_x::PartialBumpAllocator cuda_allocator;

            using self = TheCudaMatrix;

        public:

            TheCudaMatrix(std::vector<size_t> shape_vec,
                          std::vector<size_t> focal_sz_vec,
                          std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                          std::vector<size_t> rotation_sz_vec,
                          std::vector<double> parameter_bound_ratio_vec,
                          std::vector<tensor_std_float_t> shape_coeff_vec,
                          size_t base_shape_coeff_sz,
                          std::optional<size_t> deviation_operation_window): shape_vec(std::move(shape_vec)),
                                                                             focal_sz_vec(std::move(focal_sz_vec)),
                                                                             focal_suffix_map(std::move(focal_suffix_map)),
                                                                             rotation_sz_vec(std::move(rotation_sz_vec)),
                                                                             parameter_bound_ratio_vec(std::move(parameter_bound_ratio_vec)),
                                                                             shape_coeff_vec(std::move(shape_coeff_vec)),
                                                                             base_shape_coeff_sz(base_shape_coeff_sz),
                                                                             deviation_operation_window(deviation_operation_window),
                                                                             cuda_resource(std::nullopt),
                                                                             is_set_update_available(false),
                                                                             cuda_allocator(){}

            auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                if (matrix_vec.empty())
                {
                    return {};
                }

                size_t discretization_sz    = matrix_vec.size();

                if (this->deviation_operation_window.has_value())
                {
                    discretization_sz = std::max(size_t{1}, this->deviation_operation_window.value());
                }

                auto rs_vec         = std::vector<std::shared_ptr<tensor_model::Matrix>>{};
                size_t iteration_sz = matrix_vec.size() / discretization_sz + size_t{matrix_vec.size() % discretization_sz != 0u};

                for (size_t i = 0u; i < iteration_sz; ++i)
                {
                    size_t first    = i * discretization_sz;
                    size_t last     = std::min(matrix_vec.size(), static_cast<size_t>((i + 1) * discretization_sz));

                    std::vector<std::shared_ptr<tensor_model::Matrix>> rs_vec_inc   = this->project_helper(std::next(rs_vec.data(), first), last - first);
                    
                    rs_vec.insert(rs_vec.end(), rs_vec_inc.begin(), rs_vec_inc.end());
                }

                return rs_vec;
            }

            auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
            {
                std::vector<tensor_std_float_t> rs{};

                std::copy(this->coeff_vec.begin(), this->coeff_vec.end(), std::back_inserter(rs));
                std::copy(this->shape_coeff_vec.begin(), this->shape_coeff_vec.end(), std::back_inserter(rs));

                return rs;
            }

            void set_coefficient_vector(const std::vector<tensor_std_float_t>& new_coeff_vec)
            {
                if (this->coeff_vec.size() + this->shape_coeff_vec.size() != new_coeff_vec.size())
                {
                    throw std::runtime_error("invalid new_coeff_vec shape");
                }

                std::vector<tensor_std_float_t> shadow_coeff_vec(this->coeff_vec.size());
                std::vector<tensor_std_float_t> shadow_shape_coeff_vec(this->shape_coeff_vec.size());

                for (size_t i = 0u; i < shadow_coeff_vec.size(); ++i)
                {
                    if (std::isnan(new_coeff_vec[i]))
                    {
                        throw std::runtime_error("invalid new_coeff_vec shape");
                    }

                    shadow_coeff_vec[i] = new_coeff_vec[i];
                }

                for (size_t i = 0u; i < shadow_shape_coeff_vec.size(); ++i)
                {
                    shadow_shape_coeff_vec[i] = shape_projection::radian_normalize(new_coeff_vec[i + shadow_coeff_vec.size()]);

                    if (std::isnan(shadow_shape_coeff_vec[i]))
                    {
                        throw std::runtime_error("invalid new_coeff_vec shape");
                    }
                }

                this->coeff_vec         = std::move(shadow_coeff_vec);
                this->shape_coeff_vec   = std::move(shadow_shape_coeff_vec);
            }

            auto clone() -> std::shared_ptr<MatrixInterface>
            {
                return std::make_shared<self>(*this);
            }

        private:
            
            auto project_helper(const std::shared_ptr<tensor_model::Matrix> * matrix_arr, size_t matrix_arr_sz) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                this->initialize_cuda_resource_if_null();
                this->update_cuda_resource();

                std::shared_ptr<local_exception_t> cuda_err                 = cuda_mangement::host_service_x::make_cuda_object<local_exception_t>(this->cuda_allocator, local_exception::WAITING_KERNEL_COMPLETE_CODE);
                std::shared_ptr<CudaDispatchables> cuda_dispatchables       = this->matrix_vec_to_cuda_dispatchables(matrix_arr, matrix_arr_sz);

                if (!this->cuda_resource.has_value())
                {
                    std::abort();
                }

                matrix_transform(cuda_dispatchables->inp_cuda_matrix_arr, matrix_arr_sz,
                                 *this->cuda_resource->matrix_shape_vec,

                                 cuda_dispatchables->out_cuda_matrix_arr,

                                 *this->cuda_resource->focal_sz_vec,
                                 *this->cuda_resource->focal_suffix_map,
                                 *this->cuda_resource->rotation_sz_vec,
                                 *this->cuda_reosurce->parameter_bound_ratio_vec,
                                
                                 this->base_shape_coeff_sz,
                                 this->cuda_resource->logit_cuda_arr.get(), nullptr, this->shape_coeff_vec.size(),
                                
                                 cuda_err.get());

                local_exception_t host_err  = cuda_management::host_service::read_cuda_object(cuda_err);
                taylor_matrix::cuda_matrix::local_exception::throw_error_code(host_err);

                return this->cuda_dispatchables_to_matrix_vec(cuda_dispatchables->out_cuda_matrix_arr, matrix_arr_sz);
            }

            void initialize_cuda_resource_if_null()
            {
                static_assert(std::endian::native == std::endian::little);

                if (this->cuda_resource.has_value())
                {
                    return;
                }

                CudaResource tmp_cuda_resource              = {};

                tmp_cuda_resource.matrix_shape_vec          = cuda_management::host_service::to_cuda_dgbuf(this->shape_vec);
                tmp_cuda_resource.focal_sz_vec              = cuda_management::host_service::to_cuda_dgbuf(this->focal_sz_vec);
                tmp_cuda_resource.focal_suffix_map          = cuda_management::host_service::to_cuda_dgbuf(this->focal_suffix_map);
                tmp_cuda_resource.rotation_sz_vec           = cuda_management::host_service::to_cuda_dgbuf(this->rotation_sz_vec);
                tmp_cuda_resource.parameter_bound_ratio_vec = cuda_management::host_service::to_cuda_dgbuf(this->parameter_bound_ratio_vec);
                tmp_cuda_resource.logit_cuda_arr            = std::static_pointer_cast<tensor_model::tensor_std_float_t[]>(std::static_pointer_cast<void>(cuda_management::host_service::make_cuda_buffer_from_host_view(std::string_view(static_cast<char *>(static_cast<void *>(this->shape_coeff_vec.data())),
                                                                                                                                                                                                                                          this->shape_coeff_vec.size() * sizeof(tensor_std_float_t)))));

                this->cuda_resource                         = tmp_cuda_resource;
            }

            void update_cuda_resource()
            {

            }

            auto matrix_to_cuda_dispatchable(const std::shared_ptr<tensor_model::Matrix>& matrix) -> std::shared_ptr<CudaDispatchable>
            {

            }

            auto matrix_vec_to_cuda_dispatchables(const std::shared_ptr<tensor_model::Matrix> * matrix_arr, size_t matrix_arr_sz) -> std::shared_ptr<CudaDispatchables>
            {

            }
    };
}

#endif