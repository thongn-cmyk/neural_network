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
    using namespace taylor_matrix::cuda_matrix::tensor_matrix_forward;

    using tensor_std_float_t                = tensor_model::tensor_std_float_t;
    using cuda_matrix_kernel_exception_t    = uint8_t;

    class TheCudaMatrix: public virtual the_matrix::MatrixInterface
    {
        private:

            struct CudaResource
            {
                std::shared_ptr<MatrixShapeVector> matrix_shape_vec;
                std::shared_ptr<FocalSizeVector> focal_sz_vec;
                std::shared_ptr<SuffixMap> focal_suffix_map;
                std::shared_ptr<RotationSizeVector> rotation_sz_vec;
                std::shared_ptr<ParameterBoundRatioVector> parameter_bound_ratio_vec;

                std::shared_ptr<tensor_std_float_t[]> logit_cuda_arr;
            };

            struct CudaDispatchables
            {
                tensor_std_float_t ** inp_cuda_matrix_arr;
                tensor_std_float_t ** out_cuda_matrix_arr;
            };

            struct CudaDispatchable
            {
                tensor_std_float_t * inp_cuda_matrix;
                tensor_std_float_t * out_cuda_matrix;
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

            static inline constexpr double LOGIT_NORMALIZATION_VALUE = std::numbers::pi_v<double> * 10;

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

                    std::vector<std::shared_ptr<tensor_model::Matrix>> rs_vec_inc   = this->project_helper(std::next(rs_vec.data(), first),
                                                                                                           last - first);

                    rs_vec.insert(rs_vec.end(), rs_vec_inc.begin(), rs_vec_inc.end());
                }

                return rs_vec;
            }

            auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
            {
                return this->shape_coeff_vec;
            }

            void set_coefficient_vector(const std::vector<tensor_std_float_t>& new_coeff_vec)
            {
                if (this->shape_coeff_vec.size() != new_coeff_vec.size())
                {
                    throw std::runtime_error("invalid new_coeff_vec shape");
                }

                std::vector<tensor_std_float_t> shadow_shape_coeff_vec(this->shape_coeff_vec.size());

                for (size_t i = 0u; i < shadow_shape_coeff_vec.size(); ++i)
                {
                    shadow_shape_coeff_vec[i] = this->radian_normalize(new_coeff_vec[i + shadow_coeff_vec.size()]);

                    if (std::isnan(shadow_shape_coeff_vec[i]))
                    {
                        throw std::runtime_error("invalid new_coeff_vec shape");
                    }
                }

                this->shape_coeff_vec           = std::move(shadow_shape_coeff_vec);
                this->is_set_update_available   = true;
            }

            auto clone() -> std::shared_ptr<MatrixInterface>
            {
                return std::make_shared<self>(*this);
            }

        private:
            
            auto radian_normalize(tensor_std_float_t value) -> tensor_std_float_t
            {
                return std::remainder(std::remainder(value, static_cast<tensor_std_float_t>(LOGIT_NORMALIZATION_VALUE)) + static_cast<tensor_std_float_t>(LOGIT_NORMALIZATION_VALUE),
                                      static_cast<tensor_std_float_t>(LOGIT_NORMALIZATION_VALUE));
            }

            auto project_helper(const std::shared_ptr<tensor_model::Matrix> * matrix_arr, size_t matrix_arr_sz) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                this->initialize_cuda_resource_if_null();
                this->update_cuda_resource();

                std::shared_ptr<local_exception_t> cuda_err                 = cuda_mangement::host_service_x::make_cuda_object<local_exception_t>(this->cuda_allocator,
                                                                                                                                                  local_exception::WAITING_KERNEL_COMPLETE_CODE);
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

                return this->cuda_dispatchables_item_to_matrix_vec(cuda_dispatchables->out_cuda_matrix_arr, matrix_arr_sz);
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
                tmp_cuda_resource.logit_cuda_arr            = std::static_pointer_cast<tensor_std_float_t[]>(std::static_pointer_cast<void>(cuda_management::host_service::make_cuda_buffer_from_host_view(std::string_view(static_cast<char *>(static_cast<void *>(this->shape_coeff_vec.data())),
                                                                                                                                                                                                                            this->shape_coeff_vec.size() * sizeof(tensor_std_float_t)))));

                this->cuda_resource                         = tmp_cuda_resource;
            }

            void update_cuda_resource()
            {
                static_assert(std::endian::little == std::endian::native);

                if (!this->is_set_update_available)
                {
                    return;
                }

                this->initialize_cuda_resource_if_null();

                if (!this->cuda_resource.has_value())
                {
                    std::abort();
                }

                this->cuda_resource.logit_cuda_arr  = std::static_pointer_cast<tensor_std_float_t[]>(std::static_pointer_cast<void>(cuda_management::host_service::make_cuda_buffer_from_host_view(std::string_view(static_cast<char *>(static_cast<void *>(this->shape_coeff_vec.data())),
                                                                                                                                                                                                                    this->shape_coeff_vec.size() * sizeof(tensor_std_float_t)))));

                if (this->cuda_resource.logit_cuda_arr == nullptr)
                {
                    std::abort();
                }

                this->is_set_update_available   = false;
            }

            auto matrix_to_cuda_dispatchable(const std::shared_ptr<tensor_model::Matrix>& matrix) -> std::shared_ptr<CudaDispatchable>
            {
                std::vector<size_t> matrix_shape{};
                std::vector<tensor_std_float_t> matrix_logit_vec{};

                tensor_factory::get_shape(matrix, matrix_shape);
                tensor_factory::flatten(matrix, matrix_logit_vec);

                if (matrix_shape != this->shape_vec)
                {
                    throw std::invalid_argument("bad matrix, incompatible shape");
                }

                size_t logit_byte_sz                                = matrix_logit_vec.size() * sizeof(tensor_std_float_t);

                std::shared_ptr<char[]> inp_cuda_matrix_byte_arr    = cuda_management::host_service_x::make_cuda_buffer_from_host_view(std::string_view(static_cast<char *>(static_cast<void *>(matrix_logit_vec.data())),
                                                                                                                                                        logit_byte_sz),
                                                                                                                                       this->cuda_allocator);

                std::shared_ptr<char[]> out_cuda_matrix_byte_arr    = cuda_management::host_service_x::make_cuda_buffer_from_size(logit_byte_sz, this->cuda_allocator);

                auto immutable_holder                               = std::make_unique<std::pair<decltype(inp_cuda_matrix_byte_arr), decltype(out_cuda_matrix_byte_arr)>>(std::make_pair(std::move(inp_cuda_matrix_byte_arr),
                                                                                                                                                                                         std::move(out_cuda_matrix_byte_arr)));

                auto destructor = [holder = std::move(immutable_holder)](CudaDispatchable * obj)
                {
                    *holder = {};
                    delete obj;
                };

                return std::unique_ptr<CudaDispatchable, decltype(destructor)>(new CudaDispatchable(CudaDispatchable{.inp_cuda_matrix = static_cast<tensor_std_float_t *>(static_cast<void *>(inp_cuda_matrix_byte_arr.get())),
                                                                                                                     .out_cuda_matrix = static_cast<tensor_std_float_t *>(static_cast<void *>(out_cuda_matrix_byte_arr.get()))}),
                                                                               std::move(destructor));
            }

            auto matrix_vec_to_cuda_dispatchables(const std::shared_ptr<tensor_model::Matrix> * matrix_arr,
                                                  size_t matrix_arr_sz) -> std::shared_ptr<CudaDispatchables>
            {
                size_t rs_sz                                                            = matrix_arr_sz * 2u;
                std::unique_ptr<std::add_pointer_t<tensor_std_float_t>[]> rs            = std::make_unique<std::add_pointer_t<tensor_std_float_t>[]>(rs_sz);

                tensor_std_float_t ** inp_cuda_matrix_arr                               = rs.get();
                tensor_std_float_t ** out_cuda_amtrix_arr                               = std::next(rs.get(), matrix_arr_sz);

                std::vector<std::shared_ptr<CudaDispatchable>> dispatchable_vec         = {};

                for (size_t i = 0u; i < matrix_arr_sz; ++i)
                {
                    dispatchable_vec.push_back(this->matrix_to_cuda_dispatchable(matrix_arr[i]));

                    inp_cuda_matrix_arr[i]  = dispatchable_vec.back()->inp_cuda_matrix;
                    out_cuda_matrix_arr[i]  = dispatchable_vec.back()->out_cuda_matrix;
                }

                std::shared_ptr<char[]> cu_mem_arr                      = cuda_management::host_service_x::make_cuda_buffer_from_host_view(std::string_view(static_cast<char *>(static_cast<void *>(rs.get())),
                                                                                                                                                            rs_sz * sizeof(std::add_pointer_t<tensor_std_float_t>)),
                                                                                                                                           this->cuda_allocator);

                std::shared_ptr<std::add_pointer_t<tensor_std_float_t>[]> cu_float_arr  = std::static_pointer_cast<std::add_pointer_t<tensor_std_float_t>[]>(std::static_pointer_cast<void>(cu_mem_arr));

                tensor_std_float_t ** cu_inp_cuda_matrix_arr            = cu_float_arr.get();
                tensor_std_float_t ** cu_out_cuda_matrix_arr            = std::next(cu_float_arr.get(), matrix_arr_sz);

                auto immutable_holder                                   = std::make_unique<std::pair<decltype(dispatchable_vec), decltype(cu_float_arr)>>(std::make_pair(std::move(dispatchable_vec), std::move(cu_float_arr)));
                auto destructor                                         = [holder = std::move(immutable_holder)](CudaDispatchables * obj) noexcept
                {
                    *holder = {};
                    delete obj;
                };

                return std::unique_ptr<CudaDispatchables, decltype(destructor)>(new CudaDispatchables(CudaDispatchables{.inp_cuda_matrix_arr    = cu_inp_cuda_matrix_arr,
                                                                                                                        .out_cuda_matrix_arr    = cu_out_cuda_matrix_arr}),
                                                                                std::move(destructor));
            }

            auto cuda_dispatchables_item_to_matrix_vec(tensor_std_float_t ** flat_matrix_arr, size_t sz) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                size_t matrix_sz                                        = tensor_factory::shape_size(this->shape_vec);
                std::vector<std::shared_ptr<tensor_model::Matrix>> rs   = {};
                std::unique_ptr<tensor_std_float_t[]> matrix_logit_arr  = std::make_unique<tensor_std_float_t[]>(matrix_sz);

                for (size_t i = 0u; i < sz; ++i)
                {
                    host_service::memcpy_device_to_host(matrix_logit_arr.get(),
                                                        static_cast<const void *>(flat_matrix_arr[i]),
                                                        matrix_sz * sizeof(tensor_std_float_t));

                    rs.push_back(tensor_factory::make_matrix_from_flat_vec(this->shape_vec,
                                                                           std::vector<tensor_std_float_t>(matrix_logit_arr.get(),
                                                                                                           std::next(matrix_logit_arr.get(), matrix_sz))));
                }

                return rs;
            }
    };

    class TheCudaMatrixFactory
    {
        public:

            static inline constexpr uint8_t LOW_COMPUTE     = 0u;
            static inline constexpr uint8_t MID_COMPUTE     = 1u;
            static inline constexpr uint8_t HIGH_COMPUTE    = 2u;

            static inline constexpr uint8_t LOW_ENTROPY     = 0u;
            static inline constexpr uint8_t MID_ENTROPY     = 1u;
            static inline constexpr uint8_t HIGH_ENTROPY    = 2u;

        private:

            using self = TheCudaMatrixFactory;

            uint8_t compute_option;
            uint8_t entropy_option;

            std::optional<size_t> vector_sz;

        public:

            TheCudaMatrixFactory(): compute_option(LOW_COMPUTE),
                                    entropy_option(LOW_ENTROPY),
                                    vector_sz(std::nullopt){}

            auto set_entropy(uint8_t entropy_option) -> TheCudaMatrixFactory&
            {
                this->entropy_option = entropy_option;

                return *this;
            }

            auto set_compute(uint8_t compute_option) -> TheCudaMatrixFactory&
            {
                this->compute_option = compute_option;

                return *this;
            }

            auto set_vector_size(size_t sz) -> TheCudaMatrixFactory&
            {
                this->vector_sz = sz;

                return *this;
            }

            auto compute() -> TheCudaMatrixFactory&
            {
                return *this;
            }

            auto get_matrix_shape() -> std::vector<size_t>
            {
                return {};
            }

            auto get() -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                return {};
            }
    };
}

#endif