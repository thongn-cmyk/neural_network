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

    using std_float_t           = float_def::std_float_t;
    using tensor_std_float_t    = tensor_model::tensor_std_float_t;

    using DispatchCodeGenerator = taylor_matrix::host_matrix::dispatch_code_generator::DispatchCodeGenerator;

    template <class PromotedFloatType>
    using QuantizationMachine   = taylor_matrix::host_matrix::cubic_quantization_machine::GenericCubicInterpolationExponentialQuantizationMachine<PromotedFloatType>;

    template <class T>
    auto to_2d_array(const std::vector<std::vector<T>>& vec) -> std::shared_ptr<std::add_pointer_t<T>[]>
    {
        std::vector<std::vector<T>> tmp             = vec;
        std::unique_ptr<std::add_pointer_t<T>[]> rs = std::make_unique<std::add_pointer_t<T>[]>(tmp.size());
        T ** ptr                                    = rs.get();

        for (size_t i = 0u; i < vec.size(); ++i)
        {
            rs[i] = tmp[i].data();
        }

        std::shared_ptr<void> resource_holder_0                     = std::make_unique<std::pair<decltype(tmp), decltype(rs)>>(std::make_pair(std::move(tmp), std::move(rs)));
        std::unique_ptr<std::shared_ptr<void>> immutable_wrapper    = std::make_unique<std::shared_ptr<void>>(resource_holder_0);

        auto destructor = [resource_holder = std::move(immutable_wrapper)](T ** obj) noexcept
        {
            (void) obj;
            *resource_holder = {};
        };

        return std::unique_ptr<std::add_pointer_t<T>[], decltype(destructor)>(ptr, std::move(destructor));
    }

    template <class PromotedFloatType>
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
            std::vector<std::vector<tensor_std_float_t>> taylor_coeff_2d_vec;
            size_t cubic_discretization_sz;
            std_float_t cubic_exp_base;
            std_float_t cubic_multiplier_base;

        public:

            using self = TheHostMatrix;

            TheHostMatrix(std::vector<size_t> shape_vec,
                          std::vector<size_t> focal_sz_vec,
                          std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                          std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> accum_suffix_map,
                          std::vector<size_t> rotation_sz_vec,
                          std::vector<double> parameter_bound_ratio_vec,
                          const stdx::Tag<PromotedFloatType>,
                          bool has_process_unit_logit_reuse_tag,
                          bool has_process_group_logit_reuse_tag,
                          bool has_being_logit_reuse_tag,
                          bool has_base_matrix_logit_reuse_tag,
                          std::vector<std::vector<tensor_std_float_t>> taylor_coeff_2d_vec,                        
                          size_t cubic_discretization_sz,
                          std_float_t cubic_exp_base,
                          std_float_t cubic_multiplier_base) noexcept: shape_vec(std::move(shape_vec)),
                                                                       focal_sz_vec(std::move(focal_sz_vec)),
                                                                       focal_suffix_map(std::move(focal_suffix_map)),
                                                                       accum_suffix_map(std::move(accum_suffix_map)),
                                                                       rotation_sz_vec(std::move(rotation_sz_vec)),
                                                                       parameter_bound_ratio_vec(std::move(parameter_bound_ratio_vec)),
                                                                       has_process_unit_logit_reuse_tag(has_process_unit_logit_reuse_tag),
                                                                       has_process_group_logit_reuse_tag(has_process_group_logit_reuse_tag),
                                                                       has_being_logit_reuse_tag(has_being_logit_reuse_tag),
                                                                       has_base_matrix_logit_reuse_tag(has_base_matrix_logit_reuse_tag),
                                                                       taylor_coeff_2d_vec(std::move(taylor_coeff_2d_vec)),
                                                                       cubic_discretization_sz(cubic_discretization_sz),
                                                                       cubic_exp_base(cubic_exp_base),
                                                                       cubic_multiplier_base(cubic_multiplier_base){}

            auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                for (const auto& matrix: matrix_vec)
                {
                    if (matrix == nullptr)
                    {
                        throw std::invalid_argument("bad matrix vector, null element");
                    }
                }

                std::vector<std::shared_ptr<tensor_model::Matrix>> result_vec(matrix_vec.size());
                std::vector<std::pair<size_t, std::shared_ptr<tensor_model::Matrix>>> enumerated_matrix_vec = stdx::enumerate_vector(matrix_vec);

                std::shared_ptr<std::add_pointer_t<tensor_std_float_t>[]> taylor_coeff_2d_arr   = to_2d_array(this->taylor_coeff_2d_vec);

                size_t row_sz   = this->taylor_coeff_2d_vec.size();

                if (row_sz == 0u)
                {
                    std::abort();
                }

                auto par_func = [&](auto&& e)
                {
                    size_t taylor_coeff_arr_offset  = 0u;

                    DispatchCodeGenerator generator(e.second, row_sz);
                    QuantizationMachine<PromotedFloatType> quant_machine(this->cubic_discretization_sz,
                                                                         this->cubic_exp_base,
                                                                         this->cubic_multiplier_base);

                    result_vec[e.first] = tensor_matrix_operation::matrix_transform(e.second,
                                                                                    this->focal_sz_vec,
                                                                                    this->focal_suffix_map,
                                                                                    this->accum_suffix_map,
                                                                                    this->rotation_sz_vec,
                                                                                    this->parameter_bound_ratio_vec,

                                                                                    quant_machine,
                                                                                    taylor_coeff_2d_arr.get(), taylor_coeff_arr_offset, this->taylor_coeff_2d_vec.front().size(),

                                                                                    generator,

                                                                                    stdx::Tag<PromotedFloatType>{},
                                                                                    this->has_process_unit_logit_reuse_tag,
                                                                                    this->has_process_group_logit_reuse_tag,
                                                                                    this->has_being_logit_reuse_tag,
                                                                                    this->has_base_matrix_logit_reuse_tag);
                };

                if (true)
                {
                    async_x::sequential_parallel_launch(enumerated_matrix_vec.begin(), enumerated_matrix_vec.end(), par_func);
                }
                else
                {
                    std::for_each(enumerated_matrix_vec.begin(), enumerated_matrix_vec.end(), par_func);
                }

                return result_vec;
            }

            auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
            {
                std::vector<tensor_std_float_t> rs          = {};
                std::vector<tensor_std_float_t> taylor_vec  = this->get_taylor_coefficient_vector();

                std::copy(taylor_vec.begin(), taylor_vec.end(), std::back_inserter(rs));

                return rs;
            }

            void set_coefficient_vector(const std::vector<tensor_std_float_t>& new_coeff_vec)
            {
                size_t taylor_vec_sz    = this->get_taylor_coefficient_vector_size();
                size_t vec_sz           = taylor_vec_sz;

                if (new_coeff_vec.size() != vec_sz)
                {
                    throw std::invalid_argument("bad new coefficient vector, invalid size");
                }

                this->set_taylor_coefficient_vector(new_coeff_vec);
            }

            auto clone() -> std::shared_ptr<MatrixInterface>
            {
                return std::make_shared<self>(*this);
            }

        private:

            auto get_taylor_coefficient_vector() -> std::vector<tensor_std_float_t>
            {
                std::vector<tensor_std_float_t> rs{};

                size_t row_sz       = this->taylor_coeff_2d_vec.size();
                
                if (row_sz == 0u)
                {
                    std::abort();
                }

                size_t col_sz       = this->taylor_coeff_2d_vec.front().size();

                for (size_t i = 0u; i < row_sz; ++i)
                {
                    std::copy(this->taylor_coeff_2d_vec[i].begin(),
                              this->taylor_coeff_2d_vec[i].end(),
                              std::back_inserter(rs));
                }

                return rs;
            }

            auto get_taylor_coefficient_vector_size() -> size_t
            {
                if (this->taylor_coeff_2d_vec.size() == 0u)
                {
                    std::abort();
                }

                return this->taylor_coeff_2d_vec.size() * this->taylor_coeff_2d_vec.front().size();
            }

            void check_taylor_coefficient_vector(const std::vector<tensor_std_float_t>& coeff_vec)
            {
                if (coeff_vec.size() != this->get_taylor_coefficient_vector_size())
                {
                    throw std::invalid_argument("bad taylor coefficient vector size, mismatched size");
                }

                for (tensor_std_float_t e: coeff_vec)
                {
                    if (std::isnan(e))
                    {
                        throw std::invalid_argument("bad tensor argument, NaN");
                    }
                }
            }

            void set_taylor_coefficient_vector(const std::vector<tensor_std_float_t>& coeff_vec)
            {
                check_taylor_coefficient_vector(coeff_vec);

                size_t row_sz       = this->taylor_coeff_2d_vec.size();

                if (row_sz == 0u)
                {
                    std::abort();
                }

                size_t col_sz       = this->taylor_coeff_2d_vec.front().size();

                for (size_t i = 0u; i < row_sz; ++i)
                {
                    size_t first    = i * col_sz;
                    size_t last     = first + col_sz;

                    std::copy(std::next(coeff_vec.begin(), first),
                              std::next(coeff_vec.begin(), last),
                              this->taylor_coeff_2d_vec[i].begin());
                }
            }
    };

    //we'd need to be specific about entropy in the name and etc.
    //we can't do it like this

    class TheHostMatrixFactory
    {
        private:

            struct QuantizationData
            {
                size_t discretization_sz;
                tensor_std_float_t exp_base;
                tensor_std_float_t multiplier_base;
            };

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
                {4},
                {4, 2},
                {4, 2, 2},
                {4, 2, 2, 2}
            };

            static inline const std::vector<std::vector<size_t>> MID_TRANSFORMATION_ROTATION_VEC = 
            {
                {},
                {4},
                {4, 2},
                {4, 2, 2},
                {4, 2, 2, 2}
            };

            static inline const std::vector<std::vector<size_t>> HIGH_TRANSFORMATION_ROTATION_VEC = 
            {
                {},
                {4},
                {4, 2},
                {4, 2, 2},
                {4, 2, 2, 2}
            };

            static inline const double PARAMETER_BOUND_RATIO        = 0.0;

            static inline const size_t LOW_ENTROPY_HASH_TABLE_SZ    = 1;
            static inline const size_t MID_ENTROPY_HASH_TABLE_SZ    = 1;
            static inline const size_t HIGH_ENTROPY_HASH_TABLE_SZ   = 1;

            static inline const QuantizationData LOW_ENTROPY_QUANTIZATION_DATA  = 
            {
                .discretization_sz  = 64,
                .exp_base           = 1.2,
                .multiplier_base    = 1.0
            };

            static inline const QuantizationData MID_ENTROPY_QUANTIZATION_DATA  = 
            {
                .discretization_sz  = 256,
                .exp_base           = 1.1,
                .multiplier_base    = 1.0
            };

            static inline const QuantizationData HIGH_ENTROPY_QUANTIZATION_DATA = 
            {
                .discretization_sz  = 1024,
                .exp_base           = 1.02,
                .multiplier_base    = 1.0
            };

            template <class PromotedFloatType = tensor_std_float_t>
            static auto make_the_matrix(const std::vector<size_t>& matrix_shape,
                                        const std::vector<size_t>& focal_sz_vec,
                                        const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_suffix_map,
                                        const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& accum_suffix_map,
                                        const std::vector<size_t>& rotation_sz_vec,
                                        const std::vector<double>& parameter_bound_ratio_vec,
                                        size_t hash_table_sz,
                                        QuantizationData quant_data,
                                        const stdx::Tag<PromotedFloatType>& taylor_base_promotion_tag = stdx::Tag<PromotedFloatType>{},
                                        bool has_process_unit_logit_reuse_tag = true,
                                        bool has_process_group_logit_reuse_tag = true,
                                        bool has_being_logit_reuse_tag = true,
                                        bool has_base_matrix_logit_reuse_tag = true) -> std::unique_ptr<MatrixInterface>
            {
                constexpr size_t INITIAL_LOGIT_VEC_CAPACITY = size_t{1} << 10;
                constexpr size_t ITERATION_MULTIPLIER       = size_t{1} << 2;

                size_t current_logit_vec_capacity           = INITIAL_LOGIT_VEC_CAPACITY;

                while (true)
                {
                    try
                    {
                        std::vector<std::vector<tensor_std_float_t>> coeff_vec          = stdx::make_2d_vector<tensor_std_float_t>(hash_table_sz, current_logit_vec_capacity);
                        auto coeff_arr                                                  = to_2d_array(coeff_vec);

                        size_t coeff_vec_sz         = 0u;

                        DispatchCodeGenerator generator(tensor_matrix_operation::make_matrix_from_shape_vec(matrix_shape),
                                                        hash_table_sz);

                        QuantizationMachine<PromotedFloatType> quant_machine(quant_data.discretization_sz,
                                                                             quant_data.exp_base,
                                                                             quant_data.multiplier_base);

                        tensor_matrix_operation::matrix_transform(tensor_matrix_operation::make_matrix_from_shape_vec(matrix_shape),
                                                                  focal_sz_vec,
                                                                  focal_suffix_map,
                                                                  accum_suffix_map,
                                                                  rotation_sz_vec,
                                                                  parameter_bound_ratio_vec,
                                                                  quant_machine,
                                                                  coeff_arr.get(), coeff_vec_sz, current_logit_vec_capacity,
                                                                  generator,
                                                                  taylor_base_promotion_tag,
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
                                             taylor_base_promotion_tag,
                                             has_process_unit_logit_reuse_tag,
                                             has_process_group_logit_reuse_tag,
                                             has_being_logit_reuse_tag,
                                             has_base_matrix_logit_reuse_tag,
                                             stdx::make_2d_vector(hash_table_sz, coeff_vec_sz, 0.f),
                                             quant_data.discretization_sz,
                                             quant_data.exp_base,
                                             quant_data.multiplier_base);

                        return std::make_unique<decltype(matrix)>(std::move(matrix));
                    }
                    catch (std::invalid_argument& e){}

                    current_logit_vec_capacity *= ITERATION_MULTIPLIER;
                }
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
                                       this->get_hash_table_size(),
                                       this->get_quantization_data(),
                                       this->get_taylor_base_promotion_tag(),
                                       this->get_has_process_logit_reuse_tag(),
                                       this->get_has_process_group_logit_reuse_tag(),
                                       this->get_has_being_logit_reuse_tag(),
                                       this->get_has_base_matrix_logit_reuse_tag());
            }

        private:

            auto get_hash_table_size() -> size_t
            {
                switch (this->entropy_option)
                {
                    case LOW_ENTROPY:
                    {
                        return self::LOW_ENTROPY_HASH_TABLE_SZ;
                    }
                    case MID_ENTROPY:
                    {
                        return self::MID_ENTROPY_HASH_TABLE_SZ;
                    }
                    case HIGH_ENTROPY:
                    {
                        return self::HIGH_ENTROPY_HASH_TABLE_SZ;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

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

            auto get_quantization_data() -> QuantizationData
            {
                switch (this->entropy_option)
                {
                    case LOW_ENTROPY:
                    {
                        return self::LOW_ENTROPY_QUANTIZATION_DATA;
                    }
                    case MID_ENTROPY:
                    {
                        return self::MID_ENTROPY_QUANTIZATION_DATA;
                    }
                    case HIGH_ENTROPY:
                    {
                        return self::HIGH_ENTROPY_QUANTIZATION_DATA;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_taylor_base_coefficient_size() -> std::integral_constant<size_t, 2u>
            {
                return {};
            }

            auto get_taylor_base_promotion_tag() -> stdx::Tag<self::promoted_float_t>
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