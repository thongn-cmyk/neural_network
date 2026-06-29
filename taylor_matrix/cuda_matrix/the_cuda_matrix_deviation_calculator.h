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
#include "tensor_matrix_forward_to_deviation.h"
#include "tensor_matrix_forward.h"
#include <cuda_management/host_service.h>
#include <cuda_management/host_service_dgbuf.h>
#include <funnel/funnel.h>
#include <bit>
#include <cuda_management/host_service_x.h>
#include <serializer/trivial_serializer.h>
#include <deviation_projector/training_token_factory.h>
#include <matrix/tensor_factory.h>
#include <immutable_memory/immutable_memory.h>
#include <deviation_projector/cuda_device/host_code.h>
#include <optional>
#include <global_string_encoder/generic_encoder.h>
#include <iterator>
#include <memory>

namespace taylor_matrix::cuda_matrix::the_cuda_matrix_deviation_calculator
{
    using namespace taylor_matrix::cuda_matrix::tensor_matrix_forward_to_deviation;
    using namespace tensor_model;

    using mdc_float_t           = float_def::mdc_float_t;

    struct MatrixLayout
    {
        uint64_t inp_matrix_offset;
        uint64_t expected_matrix_offset;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(inp_matrix_offset,
                      expected_matrix_offset);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(inp_matrix_offset,
                      expected_matrix_offset);
        }
    };

    //refactor
    class ImmutableMemoryLayoutDictionary
    {
        private:

            struct Bucket
            {
                std::shared_ptr<void> immutable_reference;
                MatrixLayout matrix_layout;
            };

            std::unordered_map<uintptr_t, Bucket> matrix_layout_map;
        
        public:

            void add_matrix_layout(const std::shared_ptr<void>& immutable_reference,
                                   MatrixLayout matrix_layout)
            {
                if (immutable_reference == nullptr)
                {
                    throw std::invalid_argument("bad reference, null");
                }

                uintptr_t addr  = reinterpret_cast<uintptr_t>(immutable_reference.get());

                Bucket bucket   = Bucket
                {
                    .immutable_reference    = immutable_reference,
                    .matrix_layout          = matrix_layout
                };

                this->matrix_layout_map.insert(std::make_pair(addr, std::move(bucket)));
            }

            auto get_matrix_layout(const std::shared_ptr<void>& immutable_reference) -> std::optional<MatrixLayout>
            {
                uintptr_t addr  = reinterpret_cast<uintptr_t>(immutable_reference.get());
                auto map_ptr    = this->matrix_layout_map.find(addr);

                if (map_ptr == this->matrix_layout_map.end())
                {
                    return std::nullopt;
                }

                return map_ptr->second.matrix_layout;
            }

            void remove_matrix_layout(const std::shared_ptr<void>& immutable_reference) noexcept
            {
                uintptr_t addr  = reinterpret_cast<uintptr_t>(immutable_reference.get());
                this->matrix_layout_map.erase(addr);
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

                std::shared_ptr<std::add_pointer_t<tensor_std_float_t>[]> logit_cuda_arr;
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

            std::vector<std::vector<tensor_std_float_t>> shape_coeff_vec; //we'd consider cross-compute-node global mutable memory std::vector<std::vector<tensor_std_float_t>> in the next week, it's going to be tuff to implement  

            size_t base_shape_coeff_sz;
            size_t hash_table_sz;

            uint8_t cuda_deviation_calculator_device;

            std::optional<size_t> deviation_operation_window;
            std::optional<CudaResource> cuda_resource;

            std::shared_ptr<ImmutableMemoryLayoutDictionary> memory_layout_dict; //singleton
            cuda_management::host_service_x::PartialBumpAllocator cuda_allocator;
            std::unique_ptr<global_string_encoder::EncoderInterface> str_transformer;

            using self = TheCudaMatrixDeviationCalculator;

        public:

            TheCudaMatrixDeviationCalculator(std::vector<size_t> shape_vec,
                                             std::vector<size_t> focal_sz_vec,
                                             std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                                             std::vector<size_t> rotation_sz_vec,
                                             std::vector<double> parameter_bound_ratio_vec,
                                             std::vector<std::vector<tensor_std_float_t>> shape_coeff_vec,
                                             size_t base_shape_coeff_sz,
                                             size_t hash_table_sz,
                                             uint8_t cuda_deviation_calculator_device,
                                             std::optional<size_t> deviation_operation_window,
                                             global_string_encoder::ExternalGenericEncoderResource encoder_resource): shape_vec(std::move(shape_vec)),
                                                                                                                      focal_sz_vec(std::move(focal_sz_vec)),
                                                                                                                      focal_suffix_map(std::move(focal_suffix_map)),
                                                                                                                      rotation_sz_vec(std::move(rotation_sz_vec)),
                                                                                                                      parameter_bound_ratio_vec(std::move(parameter_bound_ratio_vec)),
                                                                                                                      shape_coeff_vec(std::move(shape_coeff_vec)),
                                                                                                                      base_shape_coeff_sz(base_shape_coeff_sz),
                                                                                                                      hash_table_sz(hash_table_sz),
                                                                                                                      cuda_deviation_calculator_device(cuda_deviation_calculator_device),
                                                                                                                      deviation_operation_window(deviation_operation_window),
                                                                                                                      cuda_resource(std::nullopt),
                                                                                                                      memory_layout_dict(std::make_shared<ImmutableMemoryLayoutDictionary>()),
                                                                                                                      cuda_allocator(),
                                                                                                                      str_transformer(std::make_unique<global_string_encoder::GenericEncoder>(encoder_resource)){}

            auto get_deviation(const std::vector<std::shared_ptr<immutable_memory::ImmutableMemoryInterface>>& token_vec) -> mdc_float_t
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

                return std::accumulate(deviation_vec.begin(), deviation_vec.end(),
                                       mdc_float_t{0},
                                       std::plus<mdc_float_t>{}) / deviation_vec.size();
            }

        private:

            struct ImmutableMemoryOnDestructionHandler: public virtual immutable_memory::OnDestructionCallbackInterface
            {
                static auto handler_id() -> size_t
                {
                    return 9520536842620048475ULL;
                }

                void callback(immutable_memory::ImmutableMemoryHolderInterface& memory) noexcept
                {
                    cuda_immutable_memory::evict_memory(memory.get_holder());
                }
            };

            class ImmutableMemoryLayoutResourceOnDestructionHandler: public virtual immutable_memory::OnDestructionCallbackInterface
            {
                private:

                    std::shared_ptr<ImmutableMemoryLayoutDictionary> memory_layout_dict;
                
                public:

                    ImmutableMemoryLayoutResourceOnDestructionHandler(std::shared_ptr<ImmutableMemoryLayoutDictionary> memory_layout_dict): memory_layout_dict(std::move(memory_layout_dict)){}

                    void callback(immutable_memory::ImmutableMemoryHolderInterface& memory) noexcept
                    {
                        this->memory_layout_dict->remove_matrix_layout(memory.get_holder());
                    }
            };

            auto get_deviation_helper(const std::shared_ptr<immutable_memory::ImmutableMemoryInterface> * token_arr, size_t token_arr_sz) -> mdc_float_t
            {
                this->initialize_cuda_resource_if_null();

                std::shared_ptr<mdc_float_t> cuda_output                        = cuda_management::host_service_x::make_cuda_object<mdc_float_t>(this->cuda_allocator);
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

                                              this->hash_table_sz);

                mdc_float_t host_output     = cuda_management::host_service::read_cuda_object(cuda_output);

                return host_output;
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
                    std::string_view logit_view                                     = std::string_view(static_cast<const char *>(static_cast<const void *>(e_vec.data())),
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

            static auto read_mem_layout(void * device_flat_buf) -> MatrixLayout
            {
                if (device_flat_buf == nullptr)
                {
                    std::abort();
                }

                constexpr size_t MEM_LAYOUT_SZ  = trivial_serializer::size(MatrixLayout{});

                std::array<char, MEM_LAYOUT_SZ> mem_layout_buf{};
                MatrixLayout mem_layout{};

                cuda_management::host_service::memcpy_device_to_host(mem_layout_buf.data(), device_flat_buf, MEM_LAYOUT_SZ);
                trivial_serializer::deserialize_into(mem_layout, mem_layout_buf.data());

                return mem_layout;
            }

            auto transform_token(std::string_view tok) -> std::string
            {
                std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>> training_pair = deviation_projector::training_token_factory::decode_training_token(this->str_transformer->encode(std::string(tok)));
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

            auto token_to_cuda_dispatchable(const std::shared_ptr<immutable_memory::ManagedImmutableMemoryInterface>& token) -> std::shared_ptr<CudaTokenDispatchable>
            {
                if (token == nullptr)
                {
                    throw std::invalid_argument("bad token, null");
                }

                std::shared_ptr<void> token_pass                                    = token->get_holder();
                std::optional<cuda_immutable_memory::MemoryReference> mem_reference = cuda_immutable_memory::acquire_memory(token_pass);

                if (!mem_reference.has_value())
                {
                    try
                    {
                        std::string transformed_token   = this->transform_token(token->get());
                        mem_reference                   = cuda_immutable_memory::cache_and_acquire_memory(token_pass, transformed_token);

                        token->_register(std::make_shared<ImmutableMemoryOnDestructionHandler>(),
                                         ImmutableMemoryOnDestructionHandler::handler_id());
                    }
                    catch (...)
                    {
                        std::abort();
                    }
                }

                tensor_std_float_t  * inp_cuda_matrix{};
                tensor_std_float_t  * expected_cuda_matrix{};

                auto [buf, sz]      = cuda_immutable_memory::get_cu_memspan(mem_reference.value());

                try
                {
                    std::optional<MatrixLayout> layout  = this->memory_layout_dict->get_matrix_layout(token_pass);

                    if (!layout.has_value())
                    {
                        layout = read_mem_layout(buf);
                        this->memory_layout_dict->add_matrix_layout(token_pass, layout.value());

                        token->_register(std::make_shared<ImmutableMemoryLayoutResourceOnDestructionHandler>(this->memory_layout_dict),
                                         std::nullopt);
                    }

                    inp_cuda_matrix                     = static_cast<tensor_std_float_t *>(static_cast<void *>(std::next(static_cast<char *>(buf), layout->inp_matrix_offset)));
                    expected_cuda_matrix                = static_cast<tensor_std_float_t *>(static_cast<void *>(std::next(static_cast<char *>(buf), layout->expected_matrix_offset)));
                }
                catch (...)
                {
                    std::abort();
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

            auto token_vec_to_cuda_dispatchables(const std::shared_ptr<immutable_memory::ImmutableMemoryInterface> * token_arr,
                                                 size_t token_arr_sz) -> std::shared_ptr<CudaTokenDispatchables>
            {
                size_t rs_sz                                                            = token_arr_sz * 2u;
                std::unique_ptr<std::add_pointer_t<tensor_std_float_t>[]> rs            = std::make_unique<std::add_pointer_t<tensor_std_float_t>[]>(rs_sz);

                tensor_std_float_t ** inp_cuda_matrix_arr                               = rs.get();
                tensor_std_float_t ** expected_cuda_matrix_arr                          = std::next(rs.get(), token_arr_sz);

                std::vector<std::shared_ptr<CudaTokenDispatchable>> dispatchable_vec    = {};

                for (size_t i = 0u; i < token_arr_sz; ++i)
                {
                    if (token_arr[i] == nullptr)
                    {
                        throw std::invalid_argument("bad token, null");
                    }

                    std::shared_ptr<immutable_memory::ManagedImmutableMemoryInterface> managed_token = std::dynamic_pointer_cast<immutable_memory::ManagedImmutableMemoryInterface>(token_arr[i]);

                    if (managed_token == nullptr)
                    {
                        throw std::invalid_argument("bad token, ManagedImmutableMemoryInterface required");
                    }

                    dispatchable_vec.push_back(this->token_to_cuda_dispatchable(std::move(managed_token)));

                    inp_cuda_matrix_arr[i]      = dispatchable_vec.back()->inp_cuda_matrix;
                    expected_cuda_matrix_arr[i] = dispatchable_vec.back()->expected_cuda_matrix;
                }

                std::shared_ptr<char[]> cu_mem_arr                  = cuda_management::host_service_x::make_cuda_buffer_from_host_view(std::string_view(static_cast<const char *>(static_cast<void *>(rs.get())),
                                                                                                                                                        rs_sz * sizeof(std::add_pointer_t<tensor_std_float_t>)),
                                                                                                                                       this->cuda_allocator);

                std::shared_ptr<std::add_pointer_t<tensor_std_float_t>[]> cu_float_arr  = std::static_pointer_cast<std::add_pointer_t<tensor_std_float_t>[]>(std::static_pointer_cast<void>(cu_mem_arr));

                tensor_std_float_t ** cu_inp_cuda_matrix_ptr        = cu_float_arr.get();
                tensor_std_float_t ** cu_expected_cuda_matrix_ptr   = std::next(cu_float_arr.get(), token_arr_sz);

                auto immutable_holder                               = std::make_unique<std::pair<decltype(dispatchable_vec), decltype(cu_float_arr)>>(std::make_pair(std::move(dispatchable_vec), std::move(cu_float_arr)));
                auto destructor                                     = [holder = std::move(immutable_holder)](CudaTokenDispatchables * obj) noexcept
                {
                    *holder = {}; //mostly to avoid undefined... I guess we could do __noinline__ and __noipa__ but it just feels safer this way...
                    delete obj;
                };

                return std::unique_ptr<CudaTokenDispatchables, decltype(destructor)>(new CudaTokenDispatchables(CudaTokenDispatchables{.inp_cuda_matrix_arr         = cu_inp_cuda_matrix_ptr,
                                                                                                                                       .expected_cuda_matrix_arr    = cu_expected_cuda_matrix_ptr}),
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
    //thing is hard to write but we'll try our best to refactor these later

    class TheCudaMatrixDeviationCalculatorFactory
    {
        public:

            static inline constexpr uint8_t LOW_COMPUTE     = 0u;
            static inline constexpr uint8_t MID_COMPUTE     = 1u;
            static inline constexpr uint8_t HIGH_COMPUTE    = 2u;

            static inline constexpr uint8_t LOW_ENTROPY     = 0u;
            static inline constexpr uint8_t MID_ENTROPY     = 1u;
            static inline constexpr uint8_t HIGH_ENTROPY    = 2u;

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
            static inline const size_t DEFAULT_BASE_SHAPE_COEFFICIENT_SZ    = 3u;

            static inline const size_t LOW_ENTROPY_HASH_TABLE_SZ            = 128;
            static inline const size_t MID_ENTROPY_HASH_TABLE_SZ            = 256;
            static inline const size_t HIGH_ENTROPY_HASH_TABLE_SZ           = 512;

            static inline const std::unordered_map<uint8_t, std::optional<size_t>> CONCURRENT_WORKER_MAP =
            {
                {LOW_COMPUTE, std::optional<size_t>(std::nullopt)},
                {MID_COMPUTE, std::optional<size_t>(std::nullopt)},
                {HIGH_COMPUTE, std::optional<size_t>(std::nullopt)}
            };

        private:

            using self = TheCudaMatrixDeviationCalculatorFactory;

            uint8_t compute_option;
            uint8_t entropy_option;

            std::optional<size_t> vector_sz;
            std::optional<std::vector<std::vector<tensor_std_float_t>>> logit_vec;

            global_string_encoder::StringTransformationRule str_transformation_rule;

            std::optional<size_t> operation_window;
            uint8_t deviation_calculator_device;

        public:

            TheCudaMatrixDeviationCalculatorFactory(): compute_option(LOW_COMPUTE),
                                                       entropy_option(LOW_ENTROPY),
                                                       vector_sz(std::nullopt),
                                                       logit_vec(std::nullopt),
                                                       str_transformation_rule(global_string_encoder::get_empty_transformation_rule()),
                                                       operation_window(std::nullopt),
                                                       deviation_calculator_device(deviation_projector::cuda_device::host_code::MEAN_SQUARE_DEVICE){}

            auto set_matrix(const CudaMatrixIdentifiable& cuda_matrix) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                return this->set_entropy(cuda_matrix.entropy_option)
                            .set_compute(cuda_matrix.compute_option)
                            .set_vector_size(cuda_matrix.vector_sz);
            }

            auto set_logit_vector(const std::vector<std::vector<tensor_std_float_t>>& logit_vec) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->logit_vec = logit_vec;

                return *this;
            }

            auto set_string_transformer_device(const global_string_encoder::StringTransformationRule& str_transformation_rule) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->str_transformation_rule = str_transformation_rule;

                return *this;
            }

            auto set_operation_window(std::optional<size_t> operation_window) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->operation_window = operation_window;

                return *this;
            }

            auto set_deviation_calculator_device(uint8_t deviation_calculator_device) -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->deviation_calculator_device = deviation_calculator_device;

                return *this;
            }

            auto get() -> std::unique_ptr<deviation_projector::GenericMatrixDeviationCalculatorInterface>
            {
                this->compute();

                return std::make_unique<TheCudaMatrixDeviationCalculator>(this->get_matrix_shape(),
                                                                          this->get_focal_size_vector(),
                                                                          this->get_focal_suffix_map(),
                                                                          this->get_rotation_size_vector(),
                                                                          this->get_parameter_bound_ratio_vector(),
                                                                          this->get_shape_coefficient_vector(),
                                                                          this->get_base_shape_coefficient_size(),
                                                                          this->get_hash_table_size(),
                                                                          this->deviation_calculator_device,
                                                                          this->operation_window,
                                                                          this->str_transformation_rule);
            }

        private:

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

            auto compute() -> TheCudaMatrixDeviationCalculatorFactory&
            {
                this->_compute();

                return *this;
            }

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

                for (const auto& shape: map_ptr->second)
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

            auto get_shape_coefficient_vector() -> std::vector<std::vector<tensor_std_float_t>>
            {
                if (!this->logit_vec.has_value())
                {
                    throw std::invalid_argument("bad configuration, logit vector not set");
                }

                return this->logit_vec.value();
            }

            auto get_base_shape_coefficient_size() -> size_t
            {
                return DEFAULT_BASE_SHAPE_COEFFICIENT_SZ;
            }

            auto get_deviation_operation_window() -> std::optional<size_t>
            {
                return std::nullopt;
            }

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