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

    class TheHostMatrixFactory
    {
        public:

            static inline constexpr uint8_t LOW_COMPUTE     = 0u;
            static inline constexpr uint8_t MID_COMPUTE     = 1u;
            static inline constexpr uint8_t HIGH_COMPUTE    = 2u;

            static inline constexpr uint8_t LOW_ENTROPY     = 0u;
            static inline constexpr uint8_t MID_ENTROPY     = 1u;
            static inline constexpr uint8_t HIGH_ENTROPY    = 2u;

        private:

            using self = TheHostMatrixFactory;

            uint8_t compute_option;
            uint8_t entropy_option;

            std::optional<size_t> vector_sz;

            using promoted_float_t  = tensor_model::tensor_std_float_t;

            static inline const std::vector<std::vector<size_t>> LOW_TRANSFORMATION_SHAPE_VEC = 
            {
                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    2,
                    2
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    2,
                    2 * (size_t{1} << 3)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    2,
                    2 * (size_t{1} << 6)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    2,
                    2 * (size_t{1} << 9)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    2,
                    2 * (size_t{1} << 12)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    2,
                    2 * (size_t{1} << 15)
                }
            };

            static inline const std::vector<std::vector<size_t>> MID_TRANSFORMATION_SHAPE_VEC = 
            {
                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    8,
                    2
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    8,
                    2 * (size_t{1} << 3)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    8,
                    2 * (size_t{1} << 6)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    8,
                    2 * (size_t{1} << 9)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    8,
                    2 * (size_t{1} << 12)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    8,
                    2 * (size_t{1} << 15)
                }
            };

            static inline const std::vector<std::vector<size_t>> HIGH_TRANSFORMATION_SHAPE_VEC =
            {
                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    32,
                    2
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    32,
                    2 * (size_t{1} << 3)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    32,
                    2 * (size_t{1} << 6)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    32,
                    2 * (size_t{1} << 9)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    32,
                    2 * (size_t{1} << 12)
                },

                {
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    32,
                    2 * (size_t{1} << 15)
                }
            };

            static inline const std::vector<std::vector<size_t>> LOW_TRANSFORMATION_FOCAL_VEC = 
            {
                {},
                {8},
                {8, 8},
                {8, 8, 8},
                {8, 8, 8, 8},
                {8, 8, 8, 8, 8}
            };

            static inline const std::vector<std::vector<size_t>> MID_TRANSFORMATION_FOCAL_VEC = 
            {
                {},
                {8},
                {8, 8},
                {8, 8, 8},
                {8, 8, 8, 8},
                {8, 8, 8, 8, 8}
            };

            static inline const std::vector<std::vector<size_t>> HIGH_TRANSFORMATION_FOCAL_VEC = 
            {
                {},
                {8},
                {8, 8},
                {8, 8, 8},
                {8, 8, 8, 8},
                {8, 8, 8, 8, 8}
            };

            static inline const std::vector<std::vector<size_t>> LOW_TRANSFORMATION_ROTATION_VEC = 
            {
                {},
                {4},
                {4, 2},
                {4, 2, 2},
                {4, 2, 2, 2},
                {4, 2, 2, 2, 2}
            };

            static inline const std::vector<std::vector<size_t>> MID_TRANSFORMATION_ROTATION_VEC = 
            {
                {},
                {4},
                {4, 2},
                {4, 2, 2},
                {4, 2, 2, 2},
                {4, 2, 2, 2, 2}
            };

            static inline const std::vector<std::vector<size_t>> HIGH_TRANSFORMATION_ROTATION_VEC = 
            {
                {},
                {4},
                {4, 2},
                {4, 2, 2},
                {4, 2, 2, 2},
                {4, 2, 2, 2, 2}
            };

        public:

            TheHostMatrixFactory(): compute_option(LOW_COMPUTE),
                                    entropy_option(LOW_ENTROPY),
                                    vector_sz(std::nullopt){}

            auto set_entropy(uint8_t entropy_option) -> TheHostMatrixFactory&
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

                this->entropy_option = entropy_option;

                return *this;
            }

            auto set_compute(uint8_t compute_option) -> TheHostMatrixFactory&
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

            auto set_vector_size(size_t sz) -> TheHostMatrixFactory&
            {
                size_t ceil_sz  = self::ceil_vector_size(sz);
                this->vector_sz = ceil_sz;

                return *this;
            }

            auto get_matrix_shape() -> std::vector<size_t>
            {
                if (!this->vector_sz.has_value())
                {
                    throw std::runtime_error("configuration error, vector size not set");
                }

                const std::vector<std::vector<size_t>>& shape_vec = [&]
                {
                    switch (this->entropy_option)
                    {
                        case LOW_ENTROPY:
                        {
                            return self::LOW_TRANSFORMATION_SHAPE_VEC;
                        }
                        case MID_ENTROPY:
                        {
                            return self::MID_TRANSFORMATION_SHAPE_VEC;
                        }
                        case HIGH_ENTROPY:
                        {
                            return self::HIGH_TRANSFORMATION_SHAPE_VEC;
                        }
                        default:
                        {
                            std::abort();
                        }
                    }
                }();

                for (const std::vector<size_t>& shape: shape_vec)
                {
                    if (self::shape_to_size(shape) == this->vector_sz.value())
                    {
                        return shape;
                    }
                }

                throw std::runtime_error("configuration error, vector size and entropy option mismatched");
            }

            auto get_background_semantic() -> std::string
            {
                return {};
            }

            auto get() -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                return make_the_matrix(this->get_matrix_shape(),
                                       this->get_focal_size_vector(),
                                       this->get_focal_suffix_map(),
                                       this->get_accum_suffix_map(),
                                       this->get_rotation_size_vector(),
                                       this->get_parameter_bound_ratio_vector(),
                                       this->get_pe_frequency_multiplier(),
                                       this->get_pe_amplitude_discrete_unit(),
                                       this->get_pe_dedicated_pe_size(),
                                       this->get_taylor_base_coefficient_size(),
                                       this->get_shape_base_coefficient_size(),
                                       this->get_taylor_base_promotion_tag(),
                                       this->get_shape_base_promotion_tag(),
                                       this->get_has_process_logit_reuse_tag(),
                                       this->get_has_process_group_logit_reuse_tag(),
                                       this->get_has_being_logit_reuse_tag(),
                                       this->get_has_base_matrix_logit_reuse_tag(),
                                       this->get_projection_concurrent_size());
            }

        private:

            auto ceil_vector_size(size_t sz) -> size_t
            {
                const std::vector<std::vector<size_t>>& shape_vec = [&]
                {
                    switch (this->entropy_option)
                    {
                        case LOW_ENTROPY:
                        {
                            return self::LOW_TRANSFORMATION_SHAPE_VEC;
                        }
                        case MID_ENTROPY:
                        {
                            return self::MID_TRANSFORMATION_SHAPE_VEC;
                        }
                        case HIGH_ENTROPY:
                        {
                            return self::HIGH_TRANSFORMATION_SHAPE_VEC;
                        }
                        default:
                        {
                            std::abort();
                        }
                    }
                }();

                for (const std::vector<size_t>& shape: shape_vec)
                {
                    if (self::shape_to_size(shape) >= sz)
                    {
                        return self::shape_to_size(shape);
                    }
                }

                throw std::invalid_argument("bad size, max operatable size reached");
            }

            void get_uniform_focal_map_helper(std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_rule_map,
                                              const std::vector<size_t>& focal_vec,
                                              const std::vector<size_t>& rotation_vec,
                                              const std::vector<size_t>& matrix_shape,
                                              size_t focal_idx)
            {
                if (focal_vec.empty())
                {
                    return;
                }

                if (rotation_vec.empty())
                {
                    throw std::invalid_argument("bad rotation vector access, out of bound access");
                }

                if (matrix_shape.empty())
                {
                    throw std::invalid_argument("bad matrix shape access, out of bound access");
                }

                size_t focal_group_sz       = focal_vec.front();
                size_t rotation_group_sz    = rotation_vec.front();
                size_t flat_sz              = matrix_shape.back();

                //this is way too complicated, let's say that we have 4x4x4x4, then 4x4x4x4 would recurse into 4x4x4 would recurse into 4x4 then 4, but how do we deal with this?
                //so the way we do things is that we'd want to stablize the transforming knowledge by confining that to a box of multidimensions
                //the knowledge at the time must take a part of the entire matrix, carrying a meaningful contribution to the next transformation
                //that meaningful semantics is the reason we'd want to increase the spikyness of the projection and the sigma number of the projection (which we'd prove later)
            }

            auto get_uniform_focal_map(const std::vector<size_t>& focal_vec,
                                       const std::vector<size_t>& rotation_vec,
                                       const std::vector<size_t>& matrix_shape) -> std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>
            {
                std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> rs{};
                size_t focal_idx = 0u;

                this->get_uniform_focal_map_helper(rs,
                                                   focal_vec,
                                                   rotation_vec,
                                                   matrix_shape,
                                                   focal_idx);

                return rs;
            }

            auto get_focal_size_vector() -> std::vector<size_t>
            {
                std::vector<size_t> matrix_shape = this->get_matrix_shape();

                switch (this->entropy_option)
                {
                    case LOW_ENTROPY:
                    {
                        size_t idx = std::distance(LOW_TRANSFORMATION_SHAPE_VEC.begin(), std::find(LOW_TRANSFORMATION_SHAPE_VEC.begin(), LOW_TRANSFORMATION_SHAPE_VEC.end(), matrix_shape));

                        if (idx == LOW_TRANSFORMATION_SHAPE_VEC.size())
                        {
                            std::abort();
                        }

                        return LOW_TRANSFORMATION_FOCAL_VEC[idx];
                    }
                    case MID_ENTROPY:
                    {
                        size_t idx = std::distance(MID_TRANSFORMATION_SHAPE_VEC.begin(), std::find(MID_TRANSFORMATION_SHAPE_VEC.begin(), MID_TRANSFORMATION_SHAPE_VEC.end(), matrix_shape));

                        if (idx == MID_TRANSFORMATION_SHAPE_VEC.size())
                        {
                            std::abort();
                        }

                        return MID_TRANSFORMATION_FOCAL_VEC[idx];
                    }
                    case HIGH_ENTROPY:
                    {
                        size_t idx = std::distance(HIGH_TRANSFORMATION_SHAPE_VEC.begin(), std::find(HIGH_TRANSFORMATION_SHAPE_VEC.begin(), HIGH_TRANSFORMATION_SHAPE_VEC.end(), matrix_shape));

                        if (idx == HIGH_TRANSFORMATION_SHAPE_VEC.size())
                        {
                            std::abort();
                        }

                        return HIGH_TRANSFORMATION_FOCAL_VEC[idx];
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_focal_suffix_map() -> std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>
            {
                return this->get_uniform_focal_map(this->get_focal_size_vector(),
                                                   this->get_rotation_size_vector(),
                                                   this->get_matrix_shape());
            }

            auto get_accum_suffix_map() -> std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>
            {
                return this->get_focal_suffix_map();
            }

            auto get_rotation_size_vector() -> std::vector<size_t>
            {
                std::vector<size_t> matrix_shape = this->get_matrix_shape();

                switch (this->entropy_option)
                {
                    case LOW_ENTROPY:
                    {
                        size_t idx = std::distance(LOW_TRANSFORMATION_SHAPE_VEC.begin(), std::find(LOW_TRANSFORMATION_SHAPE_VEC.begin(), LOW_TRANSFORMATION_SHAPE_VEC.end(), matrix_shape));

                        if (idx == LOW_TRANSFORMATION_SHAPE_VEC.size())
                        {
                            std::abort();
                        }

                        return LOW_TRANSFORMATION_ROTATION_VEC[idx];
                    }
                    case MID_ENTROPY:
                    {
                        size_t idx = std::distance(MID_TRANSFORMATION_SHAPE_VEC.begin(), std::find(MID_TRANSFORMATION_SHAPE_VEC.begin(), MID_TRANSFORMATION_SHAPE_VEC.end(), matrix_shape));

                        if (idx == MID_TRANSFORMATION_SHAPE_VEC.size())
                        {
                            std::abort();
                        }

                        return MID_TRANSFORMATION_ROTATION_VEC[idx];
                    }
                    case HIGH_ENTROPY:
                    {
                        size_t idx = std::distance(HIGH_TRANSFORMATION_SHAPE_VEC.begin(), std::find(HIGH_TRANSFORMATION_SHAPE_VEC.begin(), HIGH_TRANSFORMATION_SHAPE_VEC.end(), matrix_shape));

                        if (idx == HIGH_TRANSFORMATION_SHAPE_VEC.size())
                        {
                            std::abort();
                        }

                        return HIGH_TRANSFORMATION_ROTATION_VEC[idx];
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_parameter_bound_ratio_vector() -> std::vector<double>
            {
                return std::vector<double>(this->get_focal_size_vector().size(), 0.8);
            }

            auto get_pe_frequency_multiplier() -> tensor_model::tensor_std_float_t
            {
                return std::numbers::pi_v<tensor_model::tensor_std_float_t>;
            }

            auto get_pe_amplitude_discrete_unit() -> tensor_model::tensor_std_float_t
            {
                return 0.1;
            }

            auto get_pe_dedicated_pe_size() -> size_t
            {
                constexpr size_t PROCESS_GROUP_LOGIT_SZ = tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
                constexpr size_t TENTATIVE_PE_SZ        = 4u;

                return std::min(TENTATIVE_PE_SZ, PROCESS_GROUP_LOGIT_SZ);
            }

            auto get_taylor_base_coefficient_size() -> std::integral_constant<size_t, 2u>
            {
                return {};
            }

            auto get_shape_base_coefficient_size() -> std::integral_constant<size_t, 6u> //ideally, we'd want 16 for the spikyness of the chart, such is that we won't actually push the responsibility of transformation from the former layer to the latter layer
            {                                                                            //the power series would be out of range after 1 or 2 transformation, which is precisely why we want to just change certain logits
                return {};
            }

            auto get_taylor_base_promotion_tag() -> stdx::Tag<self::promoted_float_t>
            {
                return {};
            }

            auto get_shape_base_promotion_tag() -> stdx::Tag<self::promoted_float_t>
            {
                return {};
            }

            auto get_has_process_logit_reuse_tag() -> bool
            {
                return false;
            }

            auto get_has_process_group_logit_reuse_tag() -> bool
            {
                return true;
            }

            auto get_has_being_logit_reuse_tag() -> bool
            {
                return false;
            }

            auto get_has_base_matrix_logit_reuse_tag() -> bool
            {
                return true;
            }

            auto get_projection_concurrent_size() -> size_t
            {
                switch (this->compute_option)
                {
                    case LOW_COMPUTE:
                    {
                        return 1;
                    }
                    case MID_COMPUTE:
                    {
                        return 32;
                    }
                    case HIGH_COMPUTE:
                    {
                        return 1024;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            static auto shape_to_size(const std::vector<size_t>& shape) -> size_t
            {
                if (shape.empty())
                {
                    return 0u;
                }

                return std::accumulate(shape.begin(), shape.end(), size_t{1}, std::multiplies<size_t>{});
            }
    };
}

#endif