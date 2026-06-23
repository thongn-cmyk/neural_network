#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_H__

#include <memory_management/cuda_immutable_memory.h>
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
#include <cuda_management/host_service_dgbuf.h>
#include <funnel/funnel.h>
#include <matrix/tensor_factory.h>
#include <global_config/taylor_cuda_matrix_config.h>

namespace taylor_matrix::cuda_matrix::the_cuda_matrix
{
    using namespace taylor_matrix::cuda_matrix::tensor_matrix_forward;
    using namespace tensor_model;

    using cuda_matrix_kernel_exception_t    = uint8_t;

    class CudaMatrixAllocator
    {
        private:

            cuda_management::host_service_x::PartialBumpAllocator base_allocator;

            static inline constexpr size_t DEFAULT_BUMP_ALLOCATION_BUCKET_SZ    = global_config::taylor_cuda_matrix_config::BUMP_ALLOCATION_SZ;
            static inline constexpr size_t DEFAULT_BUMP_ALLOCATION_THRESHOLD    = global_config::taylor_cuda_matrix_config::BUMP_ALLOCATION_THRESHOLD;

        public:

            CudaMatrixAllocator(): base_allocator(DEFAULT_BUMP_ALLOCATION_BUCKET_SZ,
                                                  DEFAULT_BUMP_ALLOCATION_THRESHOLD){}

            auto allocate(size_t sz) -> std::shared_ptr<char[]>
            {
                return this->base_allocator.allocate(sz);
            }
    };

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

                std::shared_ptr<std::add_pointer_t<tensor_std_float_t>[]> logit_cuda_arr;
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

            std::vector<std::vector<tensor_std_float_t>> shape_coeff_vec;

            size_t base_shape_coeff_sz;
            size_t hash_table_sz;

            std::optional<size_t> operation_window;
            std::optional<CudaResource> cuda_resource;
            bool is_set_update_available;

            CudaMatrixAllocator cuda_allocator;

            using self = TheCudaMatrix;

            static inline constexpr double LOGIT_NORMALIZATION_VALUE = std::numbers::pi_v<double> * 10;

        public:

            TheCudaMatrix(std::vector<size_t> shape_vec,
                          std::vector<size_t> focal_sz_vec,
                          std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                          std::vector<size_t> rotation_sz_vec,
                          std::vector<double> parameter_bound_ratio_vec,
                          std::vector<std::vector<tensor_std_float_t>> shape_coeff_vec, //
                          size_t base_shape_coeff_sz,
                          size_t hash_table_sz,
                          std::optional<size_t> operation_window): shape_vec(std::move(shape_vec)),
                                                                   focal_sz_vec(std::move(focal_sz_vec)),
                                                                   focal_suffix_map(std::move(focal_suffix_map)),
                                                                   rotation_sz_vec(std::move(rotation_sz_vec)),
                                                                   parameter_bound_ratio_vec(std::move(parameter_bound_ratio_vec)),
                                                                   shape_coeff_vec(std::move(shape_coeff_vec)),
                                                                   base_shape_coeff_sz(base_shape_coeff_sz),
                                                                   hash_table_sz(hash_table_sz),
                                                                   operation_window(operation_window),
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

                if (this->operation_window.has_value())
                {
                    discretization_sz = std::max(size_t{1}, this->operation_window.value());
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
                std::vector<tensor_std_float_t> rs{};

                for (const std::vector<tensor_std_float_t>& e_vec: this->shape_coeff_vec)
                {
                    rs.insert(rs.end(), e_vec.begin(), e_vec.end());
                }

                return rs;
            }

            void set_coefficient_vector(const std::vector<tensor_std_float_t>& new_coeff_vec)
            {
                if (new_coeff_vec.size() != this->get_coefficient_vector().size())
                {
                    throw std::runtime_error("invalid new_coeff_vec shape");
                }

                for (size_t i = 0u; i < new_coeff_vec.size(); ++i)
                {
                    if (std::isnan(new_coeff_vec[i]))
                    {
                        throw std::invalid_argument("bad coefficient, NaN");
                    }
                }

                size_t offset = 0u;

                for (std::vector<tensor_std_float_t>& e_vec: this->shape_coeff_vec)
                {
                    std::copy(std::next(new_coeff_vec.begin(), offset),
                              std::next(new_coeff_vec.begin(), offset + e_vec.size()),
                              e_vec.begin());

                    offset += e_vec.size();
                }

                this->is_set_update_available   = true;
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

                std::shared_ptr<CudaDispatchables> cuda_dispatchables   = this->matrix_vec_to_cuda_dispatchables(matrix_arr, matrix_arr_sz);

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
                                 *this->cuda_resource->parameter_bound_ratio_vec,

                                 this->base_shape_coeff_sz,
                                 this->cuda_resource->logit_cuda_arr.get(), nullptr, this->shape_coeff_vec.size(),
                                 
                                 this->hash_table_sz);

                return this->cuda_dispatchables_item_to_matrix_vec(cuda_dispatchables->out_cuda_matrix_arr, matrix_arr_sz);
            }

            auto get_cuda_matrix_shape_vec() -> std::shared_ptr<MatrixShapeVector>
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->shape_vec);
            }

            auto get_cuda_focal_sz_vec() -> std::shared_ptr<FocalSizeVector>
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->focal_sz_vec);
            }

            auto get_cuda_focal_suffix_map() -> std::shared_ptr<SuffixMap>
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->focal_suffix_map);
            }

            auto get_cuda_rotation_sz_vec() -> std::shared_ptr<RotationSizeVector>
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->rotation_sz_vec);
            }

            auto get_cuda_parameter_bound_ratio_vec() -> std::shared_ptr<ParameterBoundRatioVector>
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->parameter_bound_ratio_vec);
            }

            auto get_cuda_shape_coeff_vec() -> std::shared_ptr<std::add_pointer_t<tensor_std_float_t>[]>
            {
                std::vector<std::shared_ptr<tensor_std_float_t[]>> cuda_2d_ptr_vec{};

                for (const auto& e_vec: this->shape_coeff_vec)
                {
                    std::string_view logit_view                                     = std::string_view(static_cast<const char *>(static_cast<void *>(e_vec.data())),
                                                                                                       e_vec.size() * sizeof(tensor_std_float_t));

                    std::shared_ptr<char[]> cuda_logit_view                         = cuda_management::host_service::make_cuda_buffer_from_host_view(logit_view);
                    std::shared_ptr<tensor_std_float_t[]> casted_cuda_logit_view    = std::static_pointer_cast<tensor_std_float_t[]>(std::static_pointer_cast<void>(cuda_logit_view));

                    cuda_2d_ptr_vec.push_back(std::move(casted_cuda_logit_view));
                }

                std::vector<uintptr_t> addr_ptr_vec{};

                for (const auto& e: cuda_2d_ptr_vec)
                {
                    addr_ptr_vec.push_back(reinterpret_cast<uintptr_t>(e.get()));
                }

                const char * addr_vec_src                       = static_cast<const char *>(static_cast<void *>(addr_ptr_vec.data())); //only works for cuda memcpy and extern immutable function
                size_t addr_vec_byte_sz                         = addr_ptr_vec.size() * sizeof(uintptr_t);
                std::string_view addr_vec_str_view              = std::string_view(addr_vec_src, addr_vec_byte_sz);

                std::shared_ptr<char[]> cuda_2d_ptr_byte_arr    = cuda_management::host_service::make_cuda_buffer_from_host_view(addr_vec_str_view);
                tensor_std_float_t ** result                    = static_cast<tensor_std_float_t **>(static_cast<void *>(cuda_2d_ptr_byte_arr.get()));

                auto immutable_holder                           = std::make_unique<std::pair<decltype(cuda_2d_ptr_vec),
                                                                                             decltype(cuda_2d_ptr_byte_arr)>>(std::make_pair(std::move(cuda_2d_ptr_vec),
                                                                                                                                             std::move(cuda_2d_ptr_byte_arr)));

                auto destructor                                 = [holder = std::move(immutable_holder)](std::add_pointer_t<tensor_std_float_t> * obj)
                {
                    *holder = {};
                    (void) obj;
                };

                return std::unique_ptr<std::add_pointer_t<tensor_std_float_t>[], decltype(destructor)>(result, std::move(destructor));
            }
        
            void initialize_cuda_resource_if_null()
            {
                static_assert(std::endian::native == std::endian::little);

                if (this->cuda_resource.has_value())
                {
                    return;
                }

                CudaResource tmp_cuda_resource              = {};

                tmp_cuda_resource.matrix_shape_vec          = this->get_cuda_matrix_shape_vec();
                tmp_cuda_resource.focal_sz_vec              = this->get_cuda_focal_sz_vec();
                tmp_cuda_resource.focal_suffix_map          = this->get_cuda_focal_suffix_map();
                tmp_cuda_resource.rotation_sz_vec           = this->get_cuda_rotation_sz_vec();
                tmp_cuda_resource.parameter_bound_ratio_vec = this->get_cuda_parameter_bound_ratio_vec();
                tmp_cuda_resource.logit_cuda_arr            = this->get_cuda_shape_coeff_vec();

                this->cuda_resource                         = std::move(tmp_cuda_resource);
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

                this->cuda_resource->logit_cuda_arr     = this->get_cuda_shape_coeff_vec();
                this->is_set_update_available           = false;
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
                char * inp_cuda_matrix_byte_ptr                     = inp_cuda_matrix_byte_arr.get();

                std::shared_ptr<char[]> out_cuda_matrix_byte_arr    = cuda_management::host_service_x::make_cuda_buffer_from_size(logit_byte_sz,
                                                                                                                                  this->cuda_allocator);
                char * out_cuda_matrix_byte_ptr                     = out_cuda_matrix_byte_arr.get();

                auto immutable_holder                               = std::make_unique<std::pair<decltype(inp_cuda_matrix_byte_arr), decltype(out_cuda_matrix_byte_arr)>>(std::make_pair(std::move(inp_cuda_matrix_byte_arr),
                                                                                                                                                                                         std::move(out_cuda_matrix_byte_arr)));

                auto destructor = [holder = std::move(immutable_holder)](CudaDispatchable * obj)
                {
                    *holder = {};
                    delete obj;
                };

                return std::unique_ptr<CudaDispatchable, decltype(destructor)>(new CudaDispatchable(CudaDispatchable{.inp_cuda_matrix = static_cast<tensor_std_float_t *>(static_cast<void *>(inp_cuda_matrix_byte_ptr)),
                                                                                                                     .out_cuda_matrix = static_cast<tensor_std_float_t *>(static_cast<void *>(out_cuda_matrix_byte_ptr))}),
                                                                               std::move(destructor));
            }

            auto matrix_vec_to_cuda_dispatchables(const std::shared_ptr<tensor_model::Matrix> * matrix_arr,
                                                  size_t matrix_arr_sz) -> std::shared_ptr<CudaDispatchables>
            {
                size_t rs_sz                                                            = matrix_arr_sz * 2u;
                std::unique_ptr<std::add_pointer_t<tensor_std_float_t>[]> rs            = std::make_unique<std::add_pointer_t<tensor_std_float_t>[]>(rs_sz);

                tensor_std_float_t ** inp_cuda_matrix_arr                               = rs.get();
                tensor_std_float_t ** out_cuda_matrix_arr                               = std::next(rs.get(), matrix_arr_sz);

                std::vector<std::shared_ptr<CudaDispatchable>> dispatchable_vec         = {};

                for (size_t i = 0u; i < matrix_arr_sz; ++i)
                {
                    dispatchable_vec.push_back(this->matrix_to_cuda_dispatchable(matrix_arr[i]));

                    inp_cuda_matrix_arr[i]  = dispatchable_vec.back()->inp_cuda_matrix;
                    out_cuda_matrix_arr[i]  = dispatchable_vec.back()->out_cuda_matrix;
                }

                std::shared_ptr<char[]> cu_mem_arr                      = cuda_management::host_service_x::make_cuda_buffer_from_host_view(std::string_view(static_cast<const char *>(static_cast<void *>(rs.get())),
                                                                                                                                                            rs_sz * sizeof(std::add_pointer_t<tensor_std_float_t>)),
                                                                                                                                           this->cuda_allocator);

                std::shared_ptr<std::add_pointer_t<tensor_std_float_t>[]> cu_float_arr  = std::static_pointer_cast<std::add_pointer_t<tensor_std_float_t>[]>(std::static_pointer_cast<void>(cu_mem_arr));

                tensor_std_float_t ** cu_inp_cuda_matrix_ptr            = cu_float_arr.get();
                tensor_std_float_t ** cu_out_cuda_matrix_ptr            = std::next(cu_float_arr.get(), matrix_arr_sz);

                auto immutable_holder                                   = std::make_unique<std::pair<decltype(dispatchable_vec), decltype(cu_float_arr)>>(std::make_pair(std::move(dispatchable_vec), std::move(cu_float_arr)));
                auto destructor                                         = [holder = std::move(immutable_holder)](CudaDispatchables * obj) noexcept
                {
                    *holder = {};
                    delete obj;
                };

                return std::unique_ptr<CudaDispatchables, decltype(destructor)>(new CudaDispatchables(CudaDispatchables{.inp_cuda_matrix_arr    = cu_inp_cuda_matrix_ptr,
                                                                                                                        .out_cuda_matrix_arr    = cu_out_cuda_matrix_ptr}),
                                                                                std::move(destructor));
            }

            auto cuda_dispatchables_item_to_matrix_vec(tensor_std_float_t ** flat_matrix_arr, size_t sz) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                size_t matrix_sz                                        = tensor_factory::shape_size(this->shape_vec);
                std::vector<std::shared_ptr<tensor_model::Matrix>> rs   = {};
                std::unique_ptr<tensor_std_float_t[]> matrix_logit_arr  = std::make_unique<tensor_std_float_t[]>(matrix_sz);

                for (size_t i = 0u; i < sz; ++i)
                {
                    cuda_management::host_service::memcpy_device_to_host(matrix_logit_arr.get(),
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

            static inline const std::unordered_map<uint8_t, std::vector<std::vector<size_t>>> TRANSFORMATION_SHAPE_MAP =
            {
                {LOW_ENTROPY, 
                {
                    {
                        size_t{1} << 1,
                        1,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 2,
                        2,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 4,
                        4,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 8,
                        4,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 16,
                        4,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    }
                }},

                {MID_ENTROPY,
                {
                    {
                        size_t{1} << 1,
                        1,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 2,
                        2,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 4,
                        4,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 8,
                        8,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 16,
                        16,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    }
                }},

                {HIGH_ENTROPY,
                {
                    {
                        size_t{1} << 1,
                        4,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 2,
                        8,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 4,
                        16,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 8,
                        32,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    },

                    {
                        size_t{1} << 16,
                        64,
                        PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                        PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                    }
                }}
            };

            static inline const std::unordered_map<uint8_t, std::vector<std::vector<size_t>>> TRANSFORMATION_FOCAL_MAP =
            {
                {LOW_ENTROPY,
                {
                    {},
                    {size_t{1} << 1},
                    {size_t{1} << 2, size_t{1} << 1},
                    {size_t{1} << 4, size_t{1} << 2, size_t{1} << 1},
                    {size_t{1} << 8, size_t{1} << 4, size_t{1} << 2, size_t{1} << 1}
                }},

                {MID_ENTROPY,
                {
                    {},
                    {size_t{1} << 1},
                    {size_t{1} << 2, size_t{1} << 1},
                    {size_t{1} << 4, size_t{1} << 2, size_t{1} << 1},
                    {size_t{1} << 8, size_t{1} << 4, size_t{1} << 2, size_t{1} << 1}
                }},

                {HIGH_ENTROPY,
                {
                    {},
                    {size_t{1} << 1},
                    {size_t{1} << 2, size_t{1} << 1},
                    {size_t{1} << 4, size_t{1} << 2, size_t{1} << 1},
                    {size_t{1} << 8, size_t{1} << 4, size_t{1} << 2, size_t{1} << 1}
                }}
            };

            static inline const std::unordered_map<uint8_t, std::vector<std::vector<size_t>>> TRANSFORMATION_ROTATION_MAP =
            {
                {LOW_ENTROPY,
                {
                    {},
                    {4},
                    {4, 2},
                    {4, 2, 2},
                    {4, 2, 2, 2}
                }},

                {MID_ENTROPY,
                {
                    {},
                    {4},
                    {4, 2},
                    {4, 2, 2},
                    {4, 2, 2, 2}
                }},

                {HIGH_ENTROPY,
                {
                    {},
                    {4},
                    {4, 2},
                    {4, 2, 2},
                    {4, 2, 2, 2}
                }}
            };

            static inline const double PARAMETER_BOUND_RATIO                = 0.0;
            static inline const size_t DEFAULT_BASE_SHAPE_COEFFICIENT_SZ    = 4u;

            static inline const size_t LOW_ENTROPY_HASH_TABLE_SZ            = 4;
            static inline const size_t MID_ENTROPY_HASH_TABLE_SZ            = 4;
            static inline const size_t HIGH_ENTROPY_HASH_TABLE_SZ           = 4;

            static inline const std::unordered_map<uint8_t, std::optional<size_t>> CONCURRENT_WORKER_MAP =
            {
                {LOW_COMPUTE, std::optional<size_t>(std::nullopt)},
                {MID_COMPUTE, std::optional<size_t>(std::nullopt)},
                {HIGH_COMPUTE, std::optional<size_t>(std::nullopt)}
            };

        public:

            TheCudaMatrixFactory(): compute_option(LOW_COMPUTE),
                                    entropy_option(LOW_ENTROPY),
                                    vector_sz(std::nullopt){}

            auto set_entropy(uint8_t entropy_option) -> TheCudaMatrixFactory&
            {
                switch (entropy_option)
                {
                    case LOW_ENTROPY:
                    case MID_ENTROPY:
                    case HIGH_ENTROPY:
                    {
                        this->entropy_option = entropy_option;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad entropy option, enumeration out of range");
                    }
                }

                return *this;
            }

            auto set_compute(uint8_t compute_option) -> TheCudaMatrixFactory&
            {
                switch (compute_option)
                {
                    case LOW_COMPUTE:
                    case MID_COMPUTE:
                    case HIGH_COMPUTE:
                    {
                        this->compute_option = compute_option;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad compute option, enumeration out of range");
                    }
                }

                return *this;
            }

            auto set_vector_size(size_t sz) -> TheCudaMatrixFactory&
            {
                this->vector_sz = sz;

                return *this;
            }

            auto compute() -> TheCudaMatrixFactory&
            {
                this->_compute();

                return *this;
            }

            auto get_matrix_shape() -> std::vector<size_t>
            {
                this->compute();

                if (!this->vector_sz.has_value())
                {
                    throw std::invalid_argument("configuration error, vector size not set");
                }

                auto map_ptr = this->TRANSFORMATION_SHAPE_MAP.find(this->entropy_option);

                if (map_ptr == this->TRANSFORMATION_SHAPE_MAP.end())
                {
                    std::abort();
                }

                for (const std::vector<size_t>& shape: map_ptr->second)
                {
                    if (tensor_factory::shape_size(shape) == this->vector_sz.value())
                    {
                        return shape;
                    }
                }

                throw std::invalid_argument("configuration error, vector size and entropy option mismatched");
            }

            auto get() -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                this->compute();

                return std::make_unique<TheCudaMatrix>(this->get_matrix_shape(),
                                                       this->get_focal_size_vector(),
                                                       this->get_focal_suffix_map(),
                                                       this->get_rotation_size_vector(),
                                                       this->get_parameter_bound_ratio_vector(),
                                                       this->get_shape_coefficient_vector(),
                                                       this->get_base_shape_coefficient_size(),
                                                       this->get_hash_table_size(),
                                                       this->get_operation_window());
            }

        private:

            void _compute()
            {
                if (!this->vector_sz.has_value())
                {
                    throw std::invalid_argument("bad configuration, vector size not set");
                }

                auto map_ptr =  this->TRANSFORMATION_SHAPE_MAP.find(this->entropy_option);

                if (map_ptr == this->TRANSFORMATION_SHAPE_MAP.end())
                {
                    std::abort();
                }

                for (const std::vector<size_t>& shape: map_ptr->second)
                {
                    if (tensor_factory::shape_size(shape) >= this->vector_sz.value())
                    {
                        this->vector_sz = tensor_factory::shape_size(shape);
                        return;
                    }
                }

                throw std::invalid_argument("bad configuration, vector size out of range");
            }

            auto get_hash_table_size() -> size_t
            {
                switch (this->entropy_option)
                {
                    case LOW_ENTROPY:
                    {
                        return LOW_ENTROPY_HASH_TABLE_SZ;
                    }
                    case MID_ENTROPY:
                    {
                        return MID_ENTROPY_HASH_TABLE_SZ;
                    }
                    case HIGH_ENTROPY:
                    {
                        return HIGH_ENTROPY_HASH_TABLE_SZ;
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto get_entropy_index() -> size_t
            {
                auto map_ptr = this->TRANSFORMATION_SHAPE_MAP.find(this->entropy_option);

                if (map_ptr == this->TRANSFORMATION_SHAPE_MAP.end())
                {
                    std::abort();
                }

                auto shape = this->get_matrix_shape();

                for (size_t i = 0u; i < map_ptr->second.size(); ++i)
                {
                    if (shape == map_ptr->second[i])
                    {
                        return i;
                    }
                }

                throw std::invalid_argument("configuration error, shape not found");
            }

            auto get_focal_size_vector() -> std::vector<size_t>
            {
                auto map_ptr = this->TRANSFORMATION_FOCAL_MAP.find(this->entropy_option);

                if (map_ptr == this->TRANSFORMATION_FOCAL_MAP.end())
                {
                    std::abort();
                }

                return map_ptr->second[this->get_entropy_index()];
            }

            auto get_row_based_suffix_rule(size_t flat_sz) -> std::vector<size_t>
            {
                std::vector<size_t> rs(flat_sz);
                std::iota(rs.begin(), rs.end(), 0u);

                return rs;
            }

            auto get_col_based_suffix_rule(size_t flat_sz) -> std::vector<size_t>
            {
                size_t sqrt_sz = std::sqrt(flat_sz);
                std::vector<size_t> rs(flat_sz);

                for (size_t i = 0u; i < sqrt_sz; ++i)
                {
                    for (size_t j = 0u; j < sqrt_sz; ++j)
                    {
                        size_t virtual_idx  = j * sqrt_sz + i;
                        size_t actual_idx   = i * sqrt_sz + j;
                        rs[actual_idx]      = virtual_idx;
                    }
                }

                return rs;
            }

            auto recurse_matrix_shape(const std::vector<size_t>& matrix_shape) -> std::vector<size_t>
            {
                auto rs = matrix_shape;

                if (rs.empty())
                {
                    std::abort();
                }

                rs.front() = std::sqrt(rs.front());

                return rs;
            }

            void get_uniform_focal_map_helper(std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_rule_map, //<matrix_array_sz> <rotation_idx> -> <matrix_array_suffix_arr>
                                              const std::vector<size_t>& rotation_vec,
                                              const std::vector<size_t>& matrix_shape)
            {
                if (rotation_vec.empty() || rotation_vec.front() == 0u)
                {
                    return;
                }

                if (matrix_shape.empty())
                {
                    throw std::invalid_argument("bad matrix shape access, out of bound access");
                }

                size_t rotation_group_sz    = rotation_vec.front();
                size_t flat_sz              = matrix_shape.front();

                for (size_t i = 0u; i < rotation_group_sz; ++i)
                {
                    if (i % 2 == 0u)
                    {
                        focal_rule_map[flat_sz][i] = {get_row_based_suffix_rule(flat_sz)};
                    }
                    else
                    {
                        focal_rule_map[flat_sz][i] = {get_col_based_suffix_rule(flat_sz)};
                    }
                }

                this->get_uniform_focal_map_helper(focal_rule_map,
                                                   {std::next(rotation_vec.begin()), rotation_vec.end()},
                                                   this->recurse_matrix_shape(matrix_shape));
            }

            auto get_uniform_focal_map(const std::vector<size_t>& rotation_vec,
                                       const std::vector<size_t>& matrix_shape) -> std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>
            {
                std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> rs{};

                this->get_uniform_focal_map_helper(rs,
                                                   rotation_vec,
                                                   matrix_shape);

                return rs;
            }

            auto get_focal_suffix_map() -> std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>
            {
                return this->get_uniform_focal_map(this->get_rotation_size_vector(),
                                                   this->get_matrix_shape());
            }

            auto get_rotation_size_vector() -> std::vector<size_t>
            {
                auto map_ptr = this->TRANSFORMATION_ROTATION_MAP.find(this->entropy_option);

                if (map_ptr == this->TRANSFORMATION_ROTATION_MAP.end())
                {
                    std::abort();
                }

                return map_ptr->second[this->get_entropy_index()];
            }

            auto get_parameter_bound_ratio_vector() -> std::vector<double>
            {
                return std::vector<double>(this->get_rotation_size_vector().size(), PARAMETER_BOUND_RATIO);
            }

            auto get_cuda_matrix_shape() -> decltype(cuda_management::host_service::to_cuda_dgbuf(std::declval<self&>().get_matrix_shape()))
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->get_matrix_shape());
            }

            auto get_cuda_focal_size_vector() -> decltype(cuda_management::host_service::to_cuda_dgbuf(std::declval<self&>().get_focal_size_vector()))
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->get_focal_size_vector());
            }

            auto get_cuda_focal_suffix_map() -> decltype(cuda_management::host_service::to_cuda_dgbuf(std::declval<self&>().get_focal_suffix_map()))
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->get_focal_suffix_map());
            }

            auto get_cuda_rotation_size_vector() -> decltype(cuda_management::host_service::to_cuda_dgbuf(std::declval<self&>().get_rotation_size_vector()))
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->get_rotation_size_vector());
            }

            auto get_cuda_parameter_bound_ratio_vector() -> decltype(cuda_management::host_service::to_cuda_dgbuf(std::declval<self&>().get_parameter_bound_ratio_vector()))
            {
                return cuda_management::host_service::to_cuda_dgbuf(this->get_parameter_bound_ratio_vector());
            }

            auto get_shape_coefficient_vector() -> std::vector<std::vector<tensor_std_float_t>>
            {
                auto shape              = this->get_cuda_matrix_shape();
                auto focal_vec          = this->get_cuda_focal_size_vector();
                auto suffix_map         = this->get_cuda_focal_suffix_map();
                auto rotation_vec       = this->get_cuda_rotation_size_vector();
                auto param_bound_vec    = this->get_cuda_parameter_bound_ratio_vector();

                size_t sz_0             = this->get_hash_table_size();
                size_t sz_1             = taylor_matrix::cuda_matrix::tensor_matrix_forward::matrix_transform_size(*shape,
                                                                                                                   *focal_vec,
                                                                                                                   *suffix_map,
                                                                                                                   *rotation_vec,
                                                                                                                   *param_bound_vec,
                                                                                                                   this->get_base_shape_coefficient_size(),
                                                                                                                   this->get_hash_table_size());

                return stdx::make_2d_vector<tensor_std_float_t>(sz_0, sz_1, 0);
            }

            auto get_base_shape_coefficient_size() -> size_t
            {
                return DEFAULT_BASE_SHAPE_COEFFICIENT_SZ;
            }

            auto get_operation_window() -> std::optional<size_t>
            {
                return std::nullopt;
            }
    };
}

#endif