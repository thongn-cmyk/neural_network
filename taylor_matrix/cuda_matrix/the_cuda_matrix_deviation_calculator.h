#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_DEVIATION_CALCULATOR_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_DEVIATION_CALCULATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <deviation_projector/generic_matrix_deviation_calculator_interface.h>
#include <memory_management/cuda_immutable_memory.h>
#include <vector>
#include <unordered_map>
#include <general_definition/float_def.h>
#include <mutex_extension/fair_mutex.h>
#include <matrix/tensor_model.h>
#include "tensor_matrix_forward_to_deviation_header.h"
#include <cuda_management/host_service_header.h>
#include <funnel/funnel.h>
#include <bit>
#include <cuda_management/host_service_x.h>
#include <serializer/trivial_serializer.h>
#include <deviation_projector/host_wrapper/one_stop_wrapper.h>
#include <matrix/tensor_model.h>
#include <matrix/tensor_factory.h>

namespace taylor_matrix::cuda_matrix::the_cuda_matrix_deviation_calculator
{
    using mdc_float_t           = float_def::mdc_float_t;
    using tensor_std_float_t    = tensor_model::tensor_std_float_t;

    using namespace taylor_matrix::cuda_matrix::tensor_matrix_forward_to_deviation;

    struct MatrixLayout
    {
        uint64_t inp_matrix_offset;
        uint64_t expected_matrix_offset;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(inp_matrix_offset, expected_matrix_offset);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(inp_matrix_offset, expected_matrix_offset);
        }
    };

    class TheCudaMatrixDeviationCalculator: public virtual deviation_projector::GenericMatrixDeviationCalculatorInterface
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

            struct CudaTokenDispatchables
            {
                tensor_std_float_t ** inp_cuda_matrix_arr;
                tensor_std_float_t ** expected_cuda_matrix_arr;
            };

            struct CudaTokenDispatchable
            {
                tensor_std_float_t * inp_cuda_matrix;
                tensor_std_float_t * expected_cuda_matrix;
            };

            std::vector<size_t> shape_vec;
            std::vector<size_t> focal_sz_vec;
            std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map;
            std::vector<size_t> rotation_sz_vec;
            std::vector<double> parameter_bound_ratio_vec;

            std::vector<tensor_std_float_t> shape_coeff_vec;
            size_t base_shape_coeff_sz;

            uint8_t cuda_deviation_calculator_device;

            std::optional<size_t> deviation_operation_window;
            std::optional<CudaResource> cuda_resource;

            cuda_management::host_service_x::PartialBumpAllocator cuda_allocator;
            std::unique_ptr<global_string_encoder::EncoderInterface> str_transformer;

            using self = TheCudaMatrixDeviationCalculator;

        public:

            TheCudaMatrixDeviationCalculator(std::vector<size_t> shape_vec,
                                             std::vector<size_t> focal_sz_vec,
                                             std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                                             std::vector<size_t> rotation_sz_vec,
                                             std::vector<double> parameter_bound_ratio_vec,
                                             std::vector<tensor_std_float_t> shape_coeff_vec,
                                             size_t base_shape_coeff_sz,
                                             uint8_t cuda_deviation_calculator_device,
                                             std::optional<size_t> deviation_operation_window,
                                             global_string_encoder::ExternalGenericEncoderResource encoder_resource): shape_vec(std::move(shape_vec)),
                                                                                                                      focal_sz_vec(std::move(focal_sz_vec)),
                                                                                                                      focal_suffix_map(std::move(focal_suffix_map)),
                                                                                                                      rotation_sz_vec(std::move(rotation_sz_vec)),
                                                                                                                      parameter_bound_ratio_vec(std::move(parameter_bound_ratio_vec)),
                                                                                                                      shape_coeff_vec(std::move(shape_coeff_vec)),
                                                                                                                      base_shape_coeff_sz(base_shape_coeff_sz),
                                                                                                                      cuda_deviation_calculator_device(cuda_deviation_calculator_device),
                                                                                                                      deviation_operation_window(deviation_operation_window),
                                                                                                                      cuda_resource(std::nullopt),
                                                                                                                      cuda_allocator(),
                                                                                                                      str_transformer(std::make_unique<global_string_encoder::GenericEncoder>(encoder_resource)){}

            auto get_deviation(const std::vector<std::shared_ptr<std::string>>& token_vec) -> mdc_float_t
            {
                if (token_vec.empty())
                {
                    return 0;
                }

                size_t discretization_sz    = token_vec.size();

                if (this->deviation_operation_window.has_value())
                {
                    discretization_sz = std::max(size_t{1}, this->deviation_operation_window.value());
                }

                std::vector<mdc_float_t> deviation_vec  = {};
                size_t iteration_sz                     = token_vec.size() / discretization_sz + size_t{token_vec.size() % discretization_sz != 0u};

                for (size_t i = 0u; i < iteration_sz; ++i)
                {
                    size_t first    = i * discretization_sz;
                    size_t last     = std::min(token_vec.size(), static_cast<size_t>((i + 1) * discretization_sz));

                    deviation_vec.push_back(this->get_deviation_helper(std::next(token_vec.data(), first), last - first));
                }

                return std::accumulate(deviation_vec.begin(), deviation_vec.end(), mdc_float_t{0}, std::plus<mdc_float_t>{}) / deviation_vec.size();
            }

        private:

            auto get_deviation_helper(const std::shared_ptr<std::string> * token_arr, size_t token_arr_sz) -> mdc_float_t
            {
                this->initialize_cuda_resource_if_null();

                std::shared_ptr<mdc_float_t> cuda_output                        = cuda_management::host_service_x::make_cuda_object<mdc_float_t>(this->cuda_allocator);
                std::shared_ptr<local_exception_t> cuda_err                     = cuda_management::host_service_x::make_cuda_object<local_exception_t>(this->cuda_allocator, local_exception::WAITING_KERNEL_COMPLETE_CODE);
                std::shared_ptr<CudaTokenDispatchables> cu_tok_dispatchables    = this->token_vec_to_cuda_dispatchables(token_arr, token_arr_sz);

                if (!this->cuda_resource.has_value())
                {
                    std::abort();
                }

                matrix_transform_to_deviation(cu_tok_dispatchables->inp_cuda_matrix_arr,
                                              cu_tok_dispatchables->expected_cuda_matrix_arr, token_arr_sz,

                                              *this->cuda_resource->matrix_shape_vec,

                                              this->cuda_deviation_calculator_device,
                                              cuda_output.get(),

                                              *this->cuda_resource->focal_sz_vec,
                                              *this->cuda_resource->focal_suffix_map,
                                              *this->cuda_resource->rotation_sz_vec,
                                              *this->cuda_resource->parameter_bound_ratio_vec,

                                              this->base_shape_coeff_sz,
                                              this->cuda_resource->logit_cuda_arr.get(), nullptr, this->shape_coeff_vec.size(),

                                              cuda_err.get());

                local_exception_t host_err  = cuda_management::host_service::read_cuda_object(cuda_err);
                taylor_matrix::cuda_matrix::local_exception::throw_error_code(host_err);
                mdc_float_t host_output     = cuda_management::host_service::read_cuda_object(cuda_output);

                return host_output;
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

            static auto read_mem_layout(void * device_flat_buf) -> MatrixLayout
            {
                constexpr size_t mem_layout_sz  = trivial_serializer::size(MatrixLayout{});
                std::array<char, mem_layout_sz> mem_layout_buf{};
                MatrixLayout mem_layout{};

                cuda_management::host_service::memcpy_device_to_host(mem_layout_buf.data(), device_flat_buf, mem_layout_sz);
                trivial_serializer::deserialize_to(mem_layout, mem_layout_buf.data());

                return mem_layout;
            }

            static auto get_input_matrix_header(void * device_flat_buf) -> tensor_std_float_t *
            {
                return static_cast<tensor_std_float_t *>(static_cast<void *>(std::next(static_cast<char *>(device_flat_buf),
                                                                             read_mem_layout(device_flat_buf).inp_matrix_offset)));
            }

            static auto get_expected_matrix_header(void * device_flat_buf) -> tensor_std_float_t *
            {
                return static_cast<tensor_std_float_t *>(static_cast<void *>(std::next(static_cast<char *>(device_flat_buf)),
                                                                             read_mem_layout(device_flat_buf).expected_matrix_offset));
            }

            auto transform_token(const std::string& tok) -> std::string
            {
                std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>> training_pair = deviation_projector::host_wrapper::decode_training_token(this->str_transformer->encode(tok));
                std::vector<size_t> inp_matrix_shape{};

                tensor_factory::get_shape(training_pair.first, inp_matrix_shape);

                if (inp_matrix_shape != this->shape_vec)
                {
                    throw std::invalid_argument("bad training token, incompatible input matrix shape");
                }

                std::vector<size_t> expected_matrix_shape{};
                tensor_factory::get_shape(training_pair.second, expected_matrix_shape);

                if (expected_matrix_shape != this->shape_vec)
                {
                    throw std::invalid_argument("bad training token, incompatible output matrix shape");
                }

                std::vector<tensor_std_float_t> inp_matrix_vec{};
                tensor_factory::flatten(training_pair.first, inp_matrix_vec);

                std::vector<tensor_std_float_t> expected_matrix_vec{};
                tensor_factory::flatten(training_pair.second, expected_matrix_vec);

                size_t total_bsz    = trivial_serializer::size(MatrixLayout{})
                                        + inp_matrix_vec.size() * sizeof(tensor_std_float_t)
                                        + expected_matrix_vec.size() * sizeof(tensor_std_float_t);

                std::string rs(total_bsz, ' ');

                MatrixLayout layout = MatrixLayout
                {
                    .inp_matrix_offset         = trivial_serializer::size(MatrixLayout{}),
                    .expected_matrix_offset    = static_cast<size_t>(trivial_serializer::size(MatrixLayout{})
                                                                        + inp_matrix_vec.size() * sizeof(tensor_std_float_t))
                };

                char * buf          = trivial_serializer::serialize_into(rs.data(), layout);

                std::memcpy(buf, inp_matrix_vec.data(), inp_matrix_vec.size() * sizeof(tensor_std_float_t));
                std::advance(buf, inp_matrix_vec.size() * sizeof(tensor_std_float_t));

                std::memcpy(buf, expected_matrix_vec.data(), expected_matrix_vec.size() * sizeof(tensor_std_float_t));
                std::advance(buf, expected_matrix_vec.size() * sizeof(tensor_std_float_t));

                return rs;
            }

            auto token_to_cuda_dispatchable(const std::shared_ptr<std::string>& token) -> std::shared_ptr<CudaTokenDispatchable>
            {
                std::optional<MemoryReference> mem_reference = cuda_immutable_memory::acquire_memory(token);

                if (!mem_reference.has_value())
                {
                    if (token == nullptr)
                    {
                        throw std::invalid_argument("bad token, null");
                    }

                    std::string transformed_token   = this->transform_token(*token);
                    mem_reference                   = cuda_immutable_memory::cache_and_acquire_memory(token, transformed_token);
                }

                auto [buf, sz] = cuda_immutable_memory::get_cu_memspan(mem_reference.value());

                tensor_std_float_t * inp_cuda_matrix{};
                tensor_std_float_t * expected_cuda_matrix{};

                try
                {
                    inp_cuda_matrix         = this->get_input_matrix_header(buf);
                    expected_cuda_matrix    = this->get_expected_matrix_header(buf);
                }
                catch (...)
                {
                    cuda_immutable_memory::release_memory(mem_reference.value());
                    throw;
                }

                auto destructor = [mem_reference](CudaTokenDispatchable * rs)
                {
                    cuda_immutable_memory::release_memory(mem_reference.value());
                    delete rs; 
                };

                try
                {
                    return std::unique_ptr<CudaTokenDispatchable, decltype(destructor)>(new CudaTokenDispatchable(CudaTokenDispatchable{.inp_cuda_matrix        = inp_cuda_matrix,
                                                                                                                                        .expected_cuda_matrix   = expected_cuda_matrix}),
                                                                                        std::move(destructor));
                }
                catch (...)
                {
                    std::abort();
                }
            }

            auto token_vec_to_cuda_dispatchables(const std::shared_ptr<std::string> * token_arr,
                                                 size_t token_arr_sz) -> std::shared_ptr<CudaTokenDispatchables>
            {
                size_t rs_sz                                                            = token_arr_sz * 2u;
                std::unique_ptr<std::add_pointer_t<tensor_std_float_t>[]> rs            = std::make_unique<std::add_pointer_t<tensor_std_float_t>[]>(rs_sz);

                tensor_std_float_t ** inp_cuda_matrix_arr                               = rs.get();
                tensor_std_float_t ** expected_cuda_matrix_arr                          = std::next(rs.get(), token_arr_sz);

                std::vector<std::shared_ptr<CudaTokenDispatchable>> dispatchable_vec    = {};

                for (size_t i = 0u; i < token_arr_sz; ++i)
                {
                    dispatchable_vec.push_back(this->token_to_cuda_dispatchable(token_arr[i]));

                    inp_cuda_matrix_arr[i]      = dispatchable_vec.back()->inp_cuda_matrix;
                    expected_cuda_matrix_arr[i] = dispatchable_vec.back()->expected_cuda_matrix;
                }

                std::shared_ptr<char[]> cu_mem_arr                  = cuda_management::host_service_x::make_cuda_buffer_from_host_view(std::string_view(static_cast<char *>(static_cast<void *>(rs.get())),
                                                                                                                                                        rs_sz * sizeof(std::add_pointer_t<tensor_std_float_t>)),
                                                                                                                                       this->cuda_allocator);

                std::shared_ptr<std::add_pointer_t<tensor_std_float_t>[]> cu_float_arr  = std::static_pointer_cast<std::add_pointer_t<tensor_std_float_t>[]>(std::static_pointer_cast<void>(cu_mem_arr));

                tensor_std_float_t ** cu_inp_cuda_matrix_arr        = cu_float_arr.get();
                tensor_std_float_t ** cu_expected_cuda_matrix_arr   = std::next(cu_float_arr.get(), token_arr_sz);

                auto immutable_holder                               = std::make_unique<std::pair<decltype(dispatchable_vec), decltype(cu_float_arr)>>(std::make_pair(std::move(dispatchable_vec), std::move(cu_float_arr)));
                auto destructor                                     = [holder = std::move(immutable_holder)](CudaTokenDispatchables * obj) noexcept
                {
                    *holder = {}; //mostly to avoid undefined... I guess we could do __noinline__ and __noipa__ but it just feels safer this way...
                    delete obj;
                };

                return std::unique_ptr<CudaTokenDispatchables, decltype(destructor)>(new CudaTokenDispatchables(CudaTokenDispatchables{.inp_cuda_matrix_arr         = cu_inp_cuda_matrix_arr,
                                                                                                                                       .expected_cuda_matrix_arr    = cu_expected_cuda_matrix_arr}),
                                                                                     std::move(destructor));
            }
    };

    struct CudaMatrixIdentifiable
    {
        uint8_t entropy_option;
        uint8_t compute_option;
        size_t vector_sz;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(entropy_option,
                      compute_option,
                      vector_sz);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(entropy_option,
                      compute_option,
                      vector_sz);
        }
    };

    //problem is we can't break encapsulations and designs, so we'd have to make it "accidentially semantically equivalent" due to design constraints

    class TheCudaMatrixDeviationCalculatorFactory
    {
        public:

            static inline constexpr uint8_t LOW_COMPUTE     = 0u;
            static inline constexpr uint8_t MID_COMPUTE     = 1u;
            static inline constexpr uint8_t HIGH_COMPUTE    = 2u;

            static inline constexpr uint8_t LOW_ENTROPY     = 0u;
            static inline constexpr uint8_t MID_ENTROPY     = 1u;
            static inline constexpr uint8_t HIGH_ENTROPY    = 2u;

        private:

            using self = TheCudaMatrixDeviationCalculatorFactory;

            uint8_t compute_option;
            uint8_t entropy_option;

            std::optional<size_t> vector_sz;
            std::optional<std::vector<tensor_std_float_t>> logit_vec;

            global_string_encoder::StringTransformationRule str_transformation_rule;

            std::optional<size_t> operational_window;
            uint8_t deviation_calculator_device;

        public:

            TheCudaMatrixDeviationCalculatorFactory(): compute_option(LOW_COMPUTE),
                                                       entropy_option(LOW_ENTROPY),
                                                       vector_sz(std::nullopt),
                                                       logit_vec(std::nullopt),
                                                       str_transformation_rule(global_string_encoder::get_empty_transformation_rule()),
                                                       operational_window(std::nullopt),
                                                       deviation_calculator_device(deviation_projector::cuda::MEAN_SQRT_DEVICE_CODE){}

            auto set_matrix(const CudaMatrixIdentifiable& cuda_matrix) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                return this->set_entropy(cuda_matrix.entropy_option)
                            .set_compute(cuda_matrix.compute_option)
                            .set_vector_size(cuda_matrix.vector_sz);
            }

            auto set_logit_vector(const std::vector<tensor_std_float_t>& logit_vec) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->logit_vec = logit_vec;

                return *this;
            }

            auto set_string_transformer_device(const global_string_encoder::StringTransformationRule& str_transformation_rule) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->str_transformation_rule = str_transformation_rule;

                return *this;
            }

            auto set_operational_window(std::optional<size_t> operational_window) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->operational_window = operational_window;

                return *this;
            }

            auto set_deviation_calculator_device(uint8_t deviation_calculator_device) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->deviation_calculator_device = deviation_calculator_device;

                return *this;
            }

            auto compute() -> TheCudaMatrixDeviationCalculatorFactory&
            {
                return *this;
            }

            auto get_matrix_shape() -> std::vector<size_t>
            {
                return {};
            }

            auto get() -> std::unique_ptr<deviation_projector::GenericMatrixDeviationCalculatorInterface>
            {
                return {};
            }

        private:

            auto set_entropy(uint8_t entropy_option) -> TheCudaMatrixDeviationCalculatorFactory&
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

            auto set_compute(uint8_t compute_option) -> TheCudaMatrixDeviationCalculatorFactory&
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

            auto set_vector_size(size_t sz) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->vector_sz = sz;

                return *this;
            }
    };
}

#endif