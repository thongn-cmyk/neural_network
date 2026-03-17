//in this guide, we'd learn about efficient cuda dispatch
//we'd aggregate 1024 dispatch order before offloading that to the kernel and do 1 cuda sync, because that's more efficient
//under the hood, we have "concurrent cuda matrix", not a cuda matrix that does concurrent operations, we'd leverage system thread to horizontally scale this

//our ticket to the genesis program is a casino program, as i said yesterday
//i still dont know why exponential + linear step + focal + random activation point is sufficient, but it's actually sufficient, just a bit costly
//but what i know for sure is we need 6 dimensions, with base of at least 8, totalling 8 ** 6 == 262144 coefficients for each base transformation, which means that we would have a really big matrix and we'd have to reuse the coefficients a lot of time

//the transformation is costly but necessary for our "AIMD" goal

#ifndef __THE_CUDA_MATRIX_H__
#define __THE_CUDA_MATRIX_H__

#include <serializer/compact_serializer.h>
#include "the_matrix_interface.h"
#include "tensor_matrix_operation.h"
#include <vector>
#include <unordered_map>
#include <general_definition/float_def.h>
#include <mutex_extension/fair_mutex.h>
#include "tensor_model.h"

namespace the_cuda_matrix
{
    using namespace the_matrix;

    using tensor_std_float_t                = tensor_model::tensor_std_float_t;
    using cuda_matrix_kernel_exception_t    = uint8_t;

    struct TheCudaHostArgument
    {
        std::vector<std::vector<tensor_std_float_t>> flattened_input_matrix_vec;
        std::vector<size_t> focal_sz_vec;
        std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map;
        std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> accum_suffix_map;
        std::vector<size_t> rotation_sz_vec;
        std::vector<double> parameter_bound_ratio_vec;
        bool has_process_unit_logit_reuse_tag;
        bool has_process_group_logit_reuse_tag;
        bool has_being_logit_reuse_tag;
        bool has_base_matrix_logit_reuse_tag;
        std::vector<tensor_std_float_t> coeff_vec;
        std::vector<tensor_std_float_t> shape_coeff_vec;
        tensor_std_float_t pe_amplitude;
        tensor_std_float_t pe_frequency_multiplier;
        tensor_std_float_t pe_amplitude_decay_rate;
        tensor_std_float_t pe_frequency_multiplier_decay_rate;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const
        {
            reflector(flattened_input_matrix_vec,
                      focal_sz_vec,
                      focal_suffix_map,
                      accum_suffix_map,
                      rotation_sz_vec,
                      parameter_bound_ratio_vec,
                      has_process_unit_logit_reuse_tag,
                      has_process_group_logit_reuse_tag,
                      has_being_logit_reuse_tag,
                      has_base_matrix_logit_reuse_tag,
                      coeff_vec,
                      shape_coeff_vec,
                      pe_amplitude,
                      pe_frequency_multiplier,
                      pe_amplitude_decay_rate,
                      pe_frequency_multiplier_decay_rate);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector)
        {
            reflector(flattened_input_matrix_vec,
                      focal_sz_vec,
                      focal_suffix_map,
                      accum_suffix_map,
                      rotation_sz_vec,
                      parameter_bound_ratio_vec,
                      has_process_unit_logit_reuse_tag,
                      has_process_group_logit_reuse_tag,
                      has_being_logit_reuse_tag,
                      has_base_matrix_logit_reuse_tag,
                      coeff_vec,
                      shape_coeff_vec,
                      pe_amplitude,
                      pe_frequency_multiplier,
                      pe_amplitude_decay_rate,
                      pe_frequency_multiplier_decay_rate);
        }
    };

    struct TheCudaCudaArgument
    {
        cu_x::cu_vector<cu_x::cu_vector<tensor_std_float_t>> flattened_input_matrix_vec;
        cu_x::cu_vector<size_t> focal_sz_vec;
        cu_x::cu_unordered_map<size_t, cu_x::cu_unordered_map<size_t, cu_x::cu_vector<cu_x::cu_vector<size_t>>>> focal_suffix_map;
        cu_x::cu_unordered_map<size_t, cu_x::cu_unordered_map<size_t, cu_x::cu_vector<cu_x::cu_vector<size_t>>>> accum_suffix_map;
        cu_x::cu_vector<size_t> rotation_sz_vec;
        cu_x::cu_vector<double> parameter_bound_ratio_vec;
        bool has_process_unit_logit_reuse_tag;
        bool has_process_group_logit_reuse_tag;
        bool has_being_logit_reuse_tag;
        bool has_base_matrix_logit_reuse_tag;
        cu_x::cu_vector<tensor_std_float_t> coeff_vec;
        cu_x::cu_vector<tensor_std_float_t> shape_coeff_vec;
        tensor_std_float_t pe_amplitude;
        tensor_std_float_t pe_frequency_multiplier;
        tensor_std_float_t pe_amplitude_decay_rate;
        tensor_std_float_t pe_frequency_multiplier_decay_rate;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const
        {
            reflector(flattened_input_matrix_vec,
                      focal_sz_vec,
                      focal_suffix_map,
                      accum_suffix_map,
                      rotation_sz_vec,
                      parameter_bound_ratio_vec,
                      has_process_unit_logit_reuse_tag,
                      has_process_group_logit_reuse_tag,
                      has_being_logit_reuse_tag,
                      has_base_matrix_logit_reuse_tag,
                      coeff_vec,
                      shape_coeff_vec,
                      pe_amplitude,
                      pe_frequency_multiplier,
                      pe_amplitude_decay_rate,
                      pe_frequency_multiplier_decay_rate);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector)
        {
            reflector(flattened_input_matrix_vec,
                      focal_sz_vec,
                      focal_suffix_map,
                      accum_suffix_map,
                      rotation_sz_vec,
                      parameter_bound_ratio_vec,
                      has_process_unit_logit_reuse_tag,
                      has_process_group_logit_reuse_tag,
                      has_being_logit_reuse_tag,
                      has_base_matrix_logit_reuse_tag,
                      coeff_vec,
                      shape_coeff_vec,
                      pe_amplitude,
                      pe_frequency_multiplier,
                      pe_amplitude_decay_rate,
                      pe_frequency_multiplier_decay_rate);
        }
    };

    struct TheCudaHostRepsonse
    {
        std::expected<std::vector<std::vector<tensor_std_float_t>>, cuda_matrix_kernel_exception_t> serialized_output_matrix_vec;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const
        {
            reflector(serialized_output_matrix_vec);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector)
        {
            reflector(serialized_output_matrix_vec);
        }
    };

    struct TheCudaCudaResponse
    {
        std::expected<cu_x::vector<cu_x::vector<tensor_std_float_t>>, cuda_matrix_kernel_exception_t> serialized_output_matrix_vec;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const
        {
            reflector(serialized_output_matrix_vec);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector)
        {
            reflector(serialized_output_matrix_vec);
        }
    };

    __global__ void cuda_matrix_genesis_launch(char * input_serialized_data, size_t input_serialized_data_sz,
                                               char * output_serialized_data, size_t output_serialized_data_cap)
    {

    }

    //we are going to do the impossible, let's aim for 10 MM $ within the next 4 weeks
    //1 BB $ within the next 3 months
    //mark my words or else i'll forever be condemned
    //we have already agreed that we only use this project for gains, not philosophical purposes, because if we ever explore in the direction, we'd be stuck in the circle of logics, which is causa sui
    //we can't fulfill our senses by defining what we know, for we only see our differences

    template <size_t TAYLOR_BASE_COEFF_SZ, size_t SHAPE_BASE_COEFF_SZ, class TaylorBasePromotedFloatType, class ShapeBasePromotedFloatType>
    class TheCudaMatrix: public virtual MatrixInterface
    {
        private:

            std::vector<size_t> shape_vec;
            std::vector<size_t> focal_sz_vec;
            std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map;
            std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> accum_suffix_map;
            std::vector<size_t> rotation_sz_vec;
            std::vector<double> parameter_bound_ratio_vec;
            bool has_process_unit_logit_reuse_tag;
            bool has_process_group_logit_reuse_tag;
            bool has_being_logit_reuse_tag;
            bool has_base_matrix_logit_reuse_tag;
            std::vector<tensor_std_float_t> coeff_vec;
            std::vector<tensor_std_float_t> shape_coeff_vec;
            tensor_std_float_t pe_amplitude;
            tensor_std_float_t pe_frequency_multiplier;
            tensor_std_float_t pe_amplitude_decay_rate;
            tensor_std_float_t pe_frequency_multiplier_decay_rate;

            using self = TheCudaMatrix;

        public:

            TheCudaMatrix(std::vector<size_t> shape_vec,
                          std::vector<size_t> focal_sz_vec,
                          std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                          std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> accum_suffix_map,
                          std::vector<size_t> rotation_sz_vec,
                          std::vector<double> parameter_bound_ratio_vec,
                          bool has_process_unit_logit_reuse_tag,
                          bool has_process_group_logit_reuse_tag,
                          bool has_being_logit_reuse_tag,
                          bool has_base_matrix_logit_reuse_tag,
                          std::vector<tensor_std_float_t> coeff_vec,
                          std::vector<tensor_std_float_t> shape_coeff_vec,
                          tensor_std_float_t pe_amplitude,
                          tensor_std_float_t pe_frequency_multiplier,
                          tensor_std_float_t pe_amplitude_decay_rate,
                          tensor_std_float_t pe_frequency_multiplier_decay_rate): shape_vec(std::move(shape_vec)),
                                                                                  focal_sz_vec(std::move(focal_sz_vec)),
                                                                                  focal_suffix_map(std::move(focal_suffix_map)),
                                                                                  accum_suffix_map(std::move(accum_suffix_map)),
                                                                                  rotation_sz_vec(std::move(rotation_sz_vec)),
                                                                                  parameter_bound_ratio_vec(std::move(parameter_bound_ratio_vec)),
                                                                                  has_process_unit_logit_reuse_tag(has_process_unit_logit_reuse_tag),
                                                                                  has_process_group_logit_reuse_tag(has_process_group_logit_reuse_tag),
                                                                                  has_being_logit_reuse_tag(has_being_logit_reuse_tag),
                                                                                  has_base_matrix_logit_reuse_tag(has_base_matrix_logit_reuse_tag),
                                                                                  coeff_vec(std::move(coeff_vec)),
                                                                                  shape_coeff_vec(std::move(shape_coeff_vec)),
                                                                                  pe_amplitude(pe_amplitude),
                                                                                  pe_frequency_multiplier(pe_frequency_multiplier),
                                                                                  pe_amplitude_decay_rate(pe_amplitude_decay_rate),
                                                                                  pe_frequency_multiplier_decay_rate(pe_frequency_multiplier_decay_rate){}

            auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                for (const auto& matrix: matrix_vec)
                {
                    if (matrix == nullptr)
                    {
                        throw std::runtime_error("invalid argument, null pointer");
                    }
                }

                TheCudaHostArgument host_argument       = this->make_host_argument_from_matrix_vec(matrix_vec);
                std::string host_serialized_argument    = dg::network_compact_serializer::integrity_serialize<std::string>(host_argument);

                std::shared_ptr<char[]> cuda_arg_buf;

                try
                {
                    cuda_arg_buf = cu_x::make_cuda_buffer_from_host_view(host_serialized_argument);
                }
                catch (std::bad_alloc& e)
                {
                    throw;
                }
                catch (std::exception& e)
                {
                    try
                    {
                        cu_x::report_corruption();
                    }
                    catch (...){}

                    throw;
                }

                size_t out_buf_sz                       = this->get_expected_output_size(matrix_vec);
                std::shared_ptr<char[]> cuda_out_buf;

                try
                {
                    cuda_out_buf = cu_x::make_cuda_buffer_from_size(out_buf_sz);
                }
                catch (std::bad_alloc& e)
                {
                    throw;
                }
                catch (std::exception& e)
                {
                    try
                    {
                        cu_x::report_corruption();
                    }
                    catch (...){}

                    throw;
                }

                std::unique_ptr<async_cuda::CudaExecutableInterface> cuda_exec = std::make_unique<InternalCudaExecutable>(cuda_arg_buf.get(), host_serialized_argument.size(),
                                                                                                                          cuda_out_buf.get(), out_buf_sz);

                try
                {
                    async_cuda::launch(std::move(cuda_exec));
                }
                catch (async_cuda_x::cuda_runtime_exception& e)
                {   
                    try
                    {
                        cu_x::report_corruption();
                    }
                    catch (...){}

                    throw;
                }
                catch (std::exception& e)
                {
                    throw;
                }

                std::shared_ptr<char[]> host_out_buf;

                try
                {
                    host_out_buf = cu_x::cuda_to_host_buf(cuda_out_buf, out_buf_sz);
                }
                catch (std::bad_alloc& e)
                {
                    throw;
                }
                catch (std::exception& e)
                {
                    try
                    {
                        cu_x::report_corruption();
                    }
                    catch (...){}

                    throw;
                }

                TheCudaHostResponse response;
                
                try
                {
                    response = dg::network_compact_serializer::integrity_deserialize<TheCudaHostResponse>(std::string_view(host_out_buf.get(), out_buf_sz));
                }
                catch (std::bad_alloc& e)
                {
                    throw;
                }
                catch (dg::network_compact_serializer::exception_space::corrupted& e)
                {
                    try
                    {
                        cu_x::report_corruption();
                    }
                    catch(...){}

                    throw;
                }
                catch (std::exception& e)
                {
                    throw;
                }

                if (!response.serialized_output_matrix_vec.has_value())
                {
                    throw std::runtime_error(verbose(response.serialized_output_matrix_vec.error()));
                }

                return this->unflatten_matrix_vec(response.serialized_output_matrix_vec);
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

            struct InternalCudaExecutable : public virtual async_cuda::CudaExecutableInterface
            {
                char * input_serialized_data;
                size_t input_serialized_data_sz;
                char * output_serialized_data;
                size_t output_serialized_data_cap;

                InternalCudaExecutable(char * input_serialized_data, size_t input_serialized_data_sz,
                                       char * output_serialized_data, size_t output_serialized_data_cap): input_serialized_data(input_serialized_data),
                                                                                                          input_serialized_data_sz(input_serialized_data_sz),
                                                                                                          output_serialized_data(output_serialized_data),
                                                                                                          output_serialized_data_cap(output_serialized_data_cap){}

                void run() noexcept
                {
                    cuda_matrix_genesis_launch<<<1, 1>>>(this->input_serialized_data, this->input_serialized_data_sz,
                                                         this->output_serialized_data, this->output_serialized_data_cap);
                }
            };

            auto flatten_matrix(const std::shared_ptr<tensor_model::Matrix>& matrix) -> std::vector<tensor_std_float_t>
            {
                std::vector<tensor_std_float_t> rs{};
                tensor_matrix_operation::flatten(matrix, rs);

                return rs;
            }

            auto unflatten_matrix(const std::vector<tensor_std_float_t>& flattened_matrix) -> std::shared_ptr<tensor_model::Matrix>
            {
                return tensor_matrix_operation::make_matrix_from_flat_vec(this->shape_vec, flattened_matrix);
            }

            auto flatten_matrix_vec(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::vector<tensor_std_float_t>>
            {
                std::vector<std::vector<tensor_std_float_t>> rs{};

                for (const auto& e: matrix_vec)
                {
                    rs.push_back(flatten_matrix(e));
                }

                return rs;
            }

            auto unflatten_matrix_vec(const std::vector<std::vector<tensor_std_float_t>>& flattened_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                std::vector<std::shared_ptr<tensor_model::Matrix>> rs{};

                for (const auto& e: flattened_vec)
                {
                    rs.push_back(unflatten_matrix(e));
                }

                return rs;
            }

            auto get_expected_output_size(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> size_t
            {
                std::vector<std::vector<tensor_std_float_t>> two_dimensional_flattened_vec = this->flatten_matrix_vec(matrix_vec);

                return dg::network_compact_serializer::integrity_size(TheCudaHostResponse{.serialized_output_matrix_vec = two_dimensional_flattened_vec});
            }

            auto make_host_argument_from_matrix_vec(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> TheCudaHostArgument
            {
                return TheCudaHostArgument
                {
                    .flattened_input_matrix_vec         = this->flatten_matrix_vec(matrix_vec),
                    .focal_sz_vec                       = this->focal_sz_vec,
                    .focal_suffix_map                   = this->focal_suffix_map,
                    .accum_suffix_map                   = this->accum_suffix_map,
                    .rotation_sz_vec                    = this->rotation_sz_vec,
                    .parameter_bound_ratio_vec          = this->parameter_bound_ratio_vec,
                    .has_process_unit_logit_reuse_tag   = this->has_process_unit_logit_reuse_tag,
                    .has_process_group_logit_reuse_tag  = this->has_process_group_logit_reuse_tag,
                    .has_being_logit_reuse_tag          = this->has_being_logit_reuse_tag,
                    .has_base_matrix_logit_reuse_tag    = this->has_base_matrix_logit_reuse_tag,
                    .coeff_vec                          = this->coeff_vec,
                    .shape_coeff_vec                    = this->shape_coeff_vec,
                    .pe_amplitude                       = this->pe_amplitude,
                    .pe_frequency_multiplier            = this->pe_frequency_multiplier,
                    .pe_amplitude_decay_rate            = this->pe_amplitude_decay_rate,
                    .pe_frequency_multiplier_decay_rate = this->pe_frequency_multiplier_decay_rate
                };
            }
    };
}

#endif