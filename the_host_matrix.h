//HEADER_CONTROL 7

#ifndef __THE_HOST_MATRIX_H__
#define __THE_HOST_MATRIX_H__

#include "async_x.h"
#include "the_matrix_interface.h"
#include "tensor_matrix_operation.h"
#include "shape_projection.h"
#include <vector>
#include <unordered_map>
#include "float_def.h"
#include <functional>
#include <algorithm>
#include <execution>

namespace the_host_matrix
{
    using namespace the_matrix;
    using tensor_std_float_t = tensor_model::tensor_std_float_t;

    template <size_t TAYLOR_BASE_COEFF_SZ, size_t SHAPE_BASE_COEFF_SZ, class TaylorBasePromotedFloatType, class ShapeBasePromotedFloatType>
    class TheHostMatrix: public virtual MatrixInterface
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
            tensor_std_float_t pe_frequency_multiplier;
            tensor_std_float_t pe_amplitude_discrete_unit;
            size_t pe_dedicated_pe_sz;
            std::optional<size_t> project_concurrent_sz;

        public:

            using self = TheHostMatrix;

            TheHostMatrix(std::vector<size_t> shape_vec,
                          std::vector<size_t> focal_sz_vec,
                          std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                          std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> accum_suffix_map,
                          std::vector<size_t> rotation_sz_vec,
                          std::vector<double> parameter_bound_ratio_vec,
                          const std::integral_constant<size_t, TAYLOR_BASE_COEFF_SZ>,
                          const std::integral_constant<size_t, SHAPE_BASE_COEFF_SZ>,
                          const stdx::Tag<TaylorBasePromotedFloatType>,
                          const stdx::Tag<ShapeBasePromotedFloatType>,
                          bool has_process_unit_logit_reuse_tag,
                          bool has_process_group_logit_reuse_tag,
                          bool has_being_logit_reuse_tag,
                          bool has_base_matrix_logit_reuse_tag,
                          std::vector<tensor_std_float_t> coeff_vec,
                          std::vector<tensor_std_float_t> shape_coeff_vec,
                          tensor_std_float_t pe_frequency_multiplier,
                          tensor_std_float_t pe_amplitude_discrete_unit,
                          size_t pe_dedicated_pe_sz,
                          std::optional<size_t> project_concurrent_sz) noexcept: shape_vec(std::move(shape_vec)),
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
                                                                                 pe_frequency_multiplier(pe_frequency_multiplier),
                                                                                 pe_amplitude_discrete_unit(pe_amplitude_discrete_unit),
                                                                                 pe_dedicated_pe_sz(pe_dedicated_pe_sz),
                                                                                 project_concurrent_sz(project_concurrent_sz){}

            auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                for (const auto& matrix: matrix_vec)
                {
                    if (matrix == nullptr)
                    {
                        throw std::invalid_argument("bad matrix vector, null element");
                    }

                    // if (tensor_matrix_operation::get_shape(matrix) != this->shape_vec)
                    // {
                    //     throw std::runtime_error("invalid argument, invalid matrix shape");
                    // }
                }

                std::vector<std::shared_ptr<tensor_model::Matrix>> result_vec(matrix_vec.size());
                std::vector<std::pair<size_t, std::shared_ptr<tensor_model::Matrix>>> enumerated_matrix_vec = stdx::enumerate_vector(matrix_vec);

                auto par_func = [&](auto&& e)
                {
                    size_t coeff_arr_offset         = 0u;
                    size_t shape_coeff_arr_offset   = 0u;

                    result_vec[e.first] = tensor_matrix_operation::matrix_transform(e.second,
                                                                                    this->focal_sz_vec,
                                                                                    this->focal_suffix_map,
                                                                                    this->accum_suffix_map,
                                                                                    this->rotation_sz_vec,
                                                                                    this->parameter_bound_ratio_vec,
                                                                                    stdx::to_size_container(std::integral_constant<size_t, TAYLOR_BASE_COEFF_SZ>{}),
                                                                                    this->coeff_vec.data(), coeff_arr_offset, this->coeff_vec.size(),
                                                                                    stdx::to_size_container(std::integral_constant<size_t, SHAPE_BASE_COEFF_SZ>{}),
                                                                                    this->shape_coeff_vec.data(), shape_coeff_arr_offset, this->shape_coeff_vec.size(),
                                                                                    this->pe_frequency_multiplier, this->pe_amplitude_discrete_unit, 0u, this->pe_dedicated_pe_sz,
                                                                                    stdx::Tag<TaylorBasePromotedFloatType>{},
                                                                                    stdx::Tag<ShapeBasePromotedFloatType>{},
                                                                                    this->has_process_unit_logit_reuse_tag,
                                                                                    this->has_process_group_logit_reuse_tag,
                                                                                    this->has_being_logit_reuse_tag,
                                                                                    this->has_base_matrix_logit_reuse_tag);
                };

                if (this->project_concurrent_sz.has_value())
                {
                    async_x::sequential_parallel_group_launch_2(enumerated_matrix_vec.begin(), enumerated_matrix_vec.end(), par_func, this->project_concurrent_sz.value());
                }
                else
                {
                    std::for_each(enumerated_matrix_vec.begin(), enumerated_matrix_vec.end(), par_func);
                }

                return result_vec;
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
                    throw std::invalid_argument("bad new coefficient vector, invalid size");
                }

                std::vector<tensor_std_float_t> shadow_coeff_vec(this->coeff_vec.size());
                std::vector<tensor_std_float_t> shadow_shape_coeff_vec(this->shape_coeff_vec.size());

                for (size_t i = 0u; i < shadow_coeff_vec.size(); ++i)
                {
                    if (std::isnan(new_coeff_vec[i]))
                    {
                        throw std::invalid_argument("bad new coefficient vector, contains NaN");
                    }

                    shadow_coeff_vec[i] = new_coeff_vec[i];
                }

                for (size_t i = 0u; i < shadow_shape_coeff_vec.size(); ++i)
                {
                    if (std::isnan(new_coeff_vec[i + shadow_coeff_vec.size()]))
                    {
                        throw std::invalid_argument("bad new coefficient vector, contains NaN");
                    }

                    shadow_shape_coeff_vec[i] = shape_projection::radian_normalize(new_coeff_vec[i + shadow_coeff_vec.size()]);

                    if (std::isnan(shadow_shape_coeff_vec[i]))
                    {
                        throw std::runtime_error("bad new coefficient vector, post_normalization contains NaN");
                    }
                }

                this->coeff_vec         = std::move(shadow_coeff_vec);
                this->shape_coeff_vec   = std::move(shadow_shape_coeff_vec);
            }

            auto clone() -> std::shared_ptr<MatrixInterface>
            {
                return std::make_shared<self>(*this);
            }
    };

    template <size_t TAYLOR_BASE_COEFF_SZ, size_t SHAPE_BASE_COEFF_SZ, class TaylorBasePromotedFloatType, class ShapeBasePromotedFloatType>
    void check_make_the_matrix_args(const std::vector<size_t>& matrix_shape,
                                    const std::vector<size_t>& focal_sz_vec,
                                    const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_suffix_map,
                                    const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& accum_suffix_map,
                                    const std::vector<size_t>& rotation_sz_vec,
                                    const std::vector<double>& parameter_bound_ratio_vec,
                                    tensor_std_float_t pe_frequency_multiplier,
                                    tensor_std_float_t pe_amplitude_discrete_unit,
                                    size_t pe_dedicated_pe_sz,
                                    const std::integral_constant<size_t, TAYLOR_BASE_COEFF_SZ>& taylor_base_coeff_sz,
                                    const std::integral_constant<size_t, SHAPE_BASE_COEFF_SZ>& shape_base_coeff_sz,
                                    const stdx::Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag,
                                    const stdx::Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag,
                                    bool has_process_unit_logit_reuse_tag,
                                    bool has_process_group_logit_reuse_tag,
                                    bool has_being_logit_reuse_tag,
                                    bool has_base_matrix_logit_reuse_tag,
                                    std::optional<size_t> project_concurrent_sz)
    {
        (void) matrix_shape;
    }

    template <size_t TAYLOR_BASE_COEFF_SZ, size_t SHAPE_BASE_COEFF_SZ,
             class TaylorBasePromotedFloatType = tensor_std_float_t, class ShapeBasePromotedFloatType = tensor_std_float_t>
    auto make_the_matrix(const std::vector<size_t>& matrix_shape,
                         const std::vector<size_t>& focal_sz_vec,
                         const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_suffix_map,
                         const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& accum_suffix_map,
                         const std::vector<size_t>& rotation_sz_vec,
                         const std::vector<double>& parameter_bound_ratio_vec,
                         tensor_std_float_t pe_frequency_multiplier,
                         tensor_std_float_t pe_amplitude_discrete_unit,
                         size_t pe_dedicated_pe_sz,
                         const std::integral_constant<size_t, TAYLOR_BASE_COEFF_SZ>& taylor_base_coeff_sz,
                         const std::integral_constant<size_t, SHAPE_BASE_COEFF_SZ>& shape_base_coeff_sz,
                         const stdx::Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = stdx::Tag<TaylorBasePromotedFloatType>{},
                         const stdx::Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = stdx::Tag<ShapeBasePromotedFloatType>{},
                         bool has_process_unit_logit_reuse_tag = true,
                         bool has_process_group_logit_reuse_tag = true,
                         bool has_being_logit_reuse_tag = true,
                         bool has_base_matrix_logit_reuse_tag = true,
                         std::optional<size_t> project_concurrent_sz = 8u) -> std::unique_ptr<MatrixInterface>
    {
        check_make_the_matrix_args(matrix_shape,
                                   focal_sz_vec,
                                   focal_suffix_map,
                                   accum_suffix_map,
                                   rotation_sz_vec,
                                   parameter_bound_ratio_vec,
                                   pe_frequency_multiplier,
                                   pe_amplitude_discrete_unit,
                                   pe_dedicated_pe_sz,
                                   taylor_base_coeff_sz,
                                   shape_base_coeff_sz,
                                   taylor_base_promotion_tag,
                                   shape_base_promotion_tag,
                                   has_process_unit_logit_reuse_tag,
                                   has_process_group_logit_reuse_tag,
                                   has_being_logit_reuse_tag,
                                   has_base_matrix_logit_reuse_tag,
                                   project_concurrent_sz);

        constexpr size_t LOGIT_VEC_CAPACITY = size_t{1} << 28u;

        std::vector<tensor_std_float_t> coeff_vec(LOGIT_VEC_CAPACITY);
        std::vector<tensor_std_float_t> shape_coeff_vec(LOGIT_VEC_CAPACITY);

        size_t coeff_vec_sz         = 0u;
        size_t shape_coeff_vec_sz   = 0u;

        tensor_matrix_operation::matrix_transform(tensor_matrix_operation::make_matrix_from_shape_vec(matrix_shape),
                                                 focal_sz_vec,
                                                 focal_suffix_map,
                                                 accum_suffix_map,
                                                 rotation_sz_vec,
                                                 parameter_bound_ratio_vec,
                                                 stdx::to_size_container(taylor_base_coeff_sz),
                                                 coeff_vec.data(), coeff_vec_sz, LOGIT_VEC_CAPACITY,
                                                 stdx::to_size_container(shape_base_coeff_sz),
                                                 shape_coeff_vec.data(), shape_coeff_vec_sz, LOGIT_VEC_CAPACITY,
                                                 pe_frequency_multiplier, pe_amplitude_discrete_unit, 0u, pe_dedicated_pe_sz,
                                                 taylor_base_promotion_tag,
                                                 shape_base_promotion_tag,
                                                 has_process_unit_logit_reuse_tag,
                                                 has_process_group_logit_reuse_tag,
                                                 has_being_logit_reuse_tag,
                                                 has_base_matrix_logit_reuse_tag);

        TheHostMatrix matrix(matrix_shape,
                             focal_sz_vec,
                             focal_suffix_map,
                             accum_suffix_map,
                             rotation_sz_vec,
                             parameter_bound_ratio_vec,
                             taylor_base_coeff_sz,
                             shape_base_coeff_sz,
                             taylor_base_promotion_tag,
                             shape_base_promotion_tag,
                             has_process_unit_logit_reuse_tag,
                             has_process_group_logit_reuse_tag,
                             has_being_logit_reuse_tag,
                             has_base_matrix_logit_reuse_tag,
                             std::vector<tensor_std_float_t>(coeff_vec_sz, 0.f),
                             std::vector<tensor_std_float_t>(shape_coeff_vec_sz, 0.f),
                             pe_frequency_multiplier, pe_amplitude_discrete_unit, pe_dedicated_pe_sz,
                             project_concurrent_sz);

        return std::make_unique<decltype(matrix)>(std::move(matrix));
    }
}

#endif