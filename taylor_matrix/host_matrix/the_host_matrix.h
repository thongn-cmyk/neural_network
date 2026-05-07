//HEADER_CONTROL 7

#ifndef __TAYLOR_MATRIX_HOST_MATRIX_THE_HOST_MATRIX_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_THE_HOST_MATRIX_H__

#include <seqpar_async/async_x.h>
#include <matrix/the_matrix_interface.h>
#include "tensor_matrix_operation.h"
#include "shape_projection.h"
#include <vector>
#include <unordered_map>
#include <general_definition/float_def.h>
#include <functional>
#include <algorithm>
#include <execution>

namespace taylor_matrix::host_matrix::the_host_matrix
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
                    size_t{1} << 1,
                    1,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 2,
                    2,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 4,
                    4,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 8,
                    4,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 16,
                    4,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                }
            };

            static inline const std::vector<std::vector<size_t>> MID_TRANSFORMATION_SHAPE_VEC = 
            {
                {
                    size_t{1} << 1,
                    1,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 2,
                    2,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 4,
                    4,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 8,
                    8,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 16,
                    16,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                }
            };

            static inline const std::vector<std::vector<size_t>> HIGH_TRANSFORMATION_SHAPE_VEC =
            {
                {
                    size_t{1} << 1,
                    4,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 2,
                    8,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 4,
                    16,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 8,
                    32,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                },

                {
                    size_t{1} << 16,
                    64,
                    tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                    tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
                }
            };

            //it's hard, that we are cell-based creature, such is that we only want the matrix transformation to be balanced which is nxn for n is an integer >= 2
            //so we'd have to pack EVERYTHING into that one-cell to be "productive" in the sense of diffracting context into other cells

            static inline const std::vector<std::vector<size_t>> LOW_TRANSFORMATION_FOCAL_VEC = 
            {
                {},
                {size_t{1} << 1},
                {size_t{1} << 2, size_t{1} << 1},
                {size_t{1} << 4, size_t{1} << 2, size_t{1} << 1},
                {size_t{1} << 8, size_t{1} << 4, size_t{1} << 2, size_t{1} << 1}
            };

            static inline const std::vector<std::vector<size_t>> MID_TRANSFORMATION_FOCAL_VEC = 
            {
                {},
                {size_t{1} << 1},
                {size_t{1} << 2, size_t{1} << 1},
                {size_t{1} << 4, size_t{1} << 2, size_t{1} << 1},
                {size_t{1} << 8, size_t{1} << 4, size_t{1} << 2, size_t{1} << 1}
            };

            static inline const std::vector<std::vector<size_t>> HIGH_TRANSFORMATION_FOCAL_VEC = 
            {
                {},
                {size_t{1} << 1},
                {size_t{1} << 2, size_t{1} << 1},
                {size_t{1} << 4, size_t{1} << 2, size_t{1} << 1},
                {size_t{1} << 8, size_t{1} << 4, size_t{1} << 2, size_t{1} << 1}
            };

            static inline const std::vector<std::vector<size_t>> LOW_TRANSFORMATION_ROTATION_VEC = 
            {
                {},
                {0},
                {4, 0},
                {4, 2, 0},
                {4, 2, 2, 0}
            };

            static inline const std::vector<std::vector<size_t>> MID_TRANSFORMATION_ROTATION_VEC = 
            {
                {},
                {0},
                {4, 0},
                {4, 2, 0},
                {4, 2, 2, 0}
            };

            static inline const std::vector<std::vector<size_t>> HIGH_TRANSFORMATION_ROTATION_VEC = 
            {
                {},
                {0},
                {4, 0},
                {4, 2, 0},
                {4, 2, 2, 0}
            };

            static inline const double PARAMETER_BOUND_RATIO        = 0.4;
            static inline const double PE_AMPLITUDE_DISCRETE_UNIT   = 0.1;
            static inline const size_t TENTATIVE_PE_SZ              = 4;

            static inline const std::unordered_map<uint8_t, std::optional<size_t>> CONCURRENT_WORKER_MAP =
            {
                {LOW_COMPUTE, std::optional<size_t>(std::nullopt)},
                {MID_COMPUTE, std::optional<size_t>(std::nullopt)},
                {HIGH_COMPUTE, std::optional<size_t>(std::nullopt)}
            };

            template <size_t TAYLOR_BASE_COEFF_SZ, size_t SHAPE_BASE_COEFF_SZ, class TaylorBasePromotedFloatType, class ShapeBasePromotedFloatType>
            static void check_make_the_matrix_args(const std::vector<size_t>& matrix_shape,
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
            static auto make_the_matrix(const std::vector<size_t>& matrix_shape,
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
                this->vector_sz = sz;

                return *this;
            }

            auto compute() -> TheHostMatrixFactory&
            {
                if (!this->vector_sz.has_value())
                {
                    throw std::invalid_argument("configuration error, vector size not set");
                }

                size_t ceil_sz  = this->ceil_vector_size(this->vector_sz.value());
                this->vector_sz = ceil_sz;

                return *this;
            }

            auto get_matrix_shape() -> std::vector<size_t>
            {
                this->compute();

                if (!this->vector_sz.has_value())
                {
                    throw std::invalid_argument("configuration error, vector size not set");
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

                throw std::invalid_argument("configuration error, vector size and entropy option mismatched");
            }

            auto get() -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                this->compute();

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
                return this->get_uniform_focal_map(this->get_rotation_size_vector(),
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
                return std::vector<double>(this->get_focal_size_vector().size(), PARAMETER_BOUND_RATIO);
            }

            auto get_pe_frequency_multiplier() -> tensor_model::tensor_std_float_t
            {
                return std::numbers::pi_v<tensor_model::tensor_std_float_t>;
            }

            auto get_pe_amplitude_discrete_unit() -> tensor_model::tensor_std_float_t
            {
                return PE_AMPLITUDE_DISCRETE_UNIT;
            }

            auto get_pe_dedicated_pe_size() -> size_t
            {
                constexpr size_t PROCESS_GROUP_LOGIT_SZ = tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;

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

            auto get_projection_concurrent_size() -> std::optional<size_t>
            {
                return CONCURRENT_WORKER_MAP.at(this->compute_option);
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