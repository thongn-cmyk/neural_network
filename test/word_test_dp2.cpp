
#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>
#include <random>
#include <functional>
#include <chrono>
#include <stl_extension/stdx.h>
#include <taylor_matrix/host_matrix/taylor_projection.h>

#include <stdint.h>
#include <stdlib.h>
#include <taylor_matrix/host_matrix/the_host_matrix.h>
#include <random>
#include <functional>
#include <algorithm>
#include <utility>
#include <numeric>
#include <type_traits>
#include <filesystem>

#include <iostream>

#include <taylor_matrix/host_matrix/the_host_matrix.h>
#include <matrix_optimizer_subsystem/coordinated_search_optimizer_engine.h>
#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <random>
#include <functional>
#include <algorithm>
#include <memory>
#include <matrix_steering_subsystem/taylor_projection.h>
#include <matrix_steering_subsystem/shape_projection.h>
#include <matrix_steering_subsystem/by_step_optimizer.h>
#include <matrix/tensor_model.h>
#include <matrix/tensor_factory.h>
#include <general_definition/float_def.h>
#include <math.h>
#include <type_traits>
#include <stl_extension/stdx.h>
#include <stl_extension/hasher.h>
#include <matrix_steering_subsystem/by_step_optimizer.h>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <general_definition/float_def.h>
#include <format>
#include <limits.h>
#include <sstream>
#include <taylor_matrix/host_matrix/dispatch_code_generator.h>
#include <taylor_matrix/host_matrix/generic_one_dimensional_cubic_interpolation.h>
#include <taylor_matrix/host_matrix/generic_two_dimensional_cubic_interpolation.h>

using namespace taylor_matrix::host_matrix::the_host_matrix;
using namespace taylor_matrix::host_matrix::tensor_matrix_operation;
using namespace float_def;

using DispatchCodeGenerator = taylor_matrix::host_matrix::dispatch_code_generator::DispatchCodeGenerator;

auto randomize_int(size_t first, size_t last) -> size_t
{
    if (first >= last)
    {
        std::abort();
    }

    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return first + randomizer() % (last - first);
}

auto randomize_bool() -> bool
{
    return randomize_int(0u, 32) < 16;
}

template <class T>
auto randomize_optional_int(size_t first, size_t last) -> std::optional<T>
{
    static_assert(std::is_unsigned_v<T>);

    if (randomize_int(0u, 1) == 0u)
    {
        return std::nullopt;
    }

    return randomize_int(first, last);
}

struct Node
{
    std::unique_ptr<Node> lhs;
    std::unique_ptr<Node> rhs;
    std::vector<float> semantic_vec;
};

struct NodeContainer
{
    std::unique_ptr<Node> root;
};

auto randomize_float(float first, float last)
{
    static auto randomizer      = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    auto real_distributor       = std::uniform_real_distribution<float>(first, last);

    return real_distributor(randomizer);
}

auto make_tree(size_t height,
               float discretization_value,
               size_t semantic_sz) -> std::unique_ptr<Node>
{
    if (height == 0u)
    {
        return nullptr;
    }

    std::vector<float> semantic_vec = {};

    for (size_t i = 0u; i < semantic_sz; ++i)
    {
        float first = discretization_value * i;
        float last  = first + discretization_value;

        semantic_vec.push_back(randomize_float(first, last));
    }

    return std::make_unique<Node>
    (
        make_tree(height - 1, discretization_value, semantic_sz),
        make_tree(height - 1, discretization_value, semantic_sz),
        std::move(semantic_vec)
    );
}

auto make_node_container(std::unique_ptr<Node>&& root) -> std::unique_ptr<NodeContainer>
{
    return std::make_unique<NodeContainer>
    (
        std::move(root)
    );
}

struct Projection
{
    std::vector<float> x;
    float y;
};

auto make_projection(const std::unique_ptr<Node>& root) -> std::vector<Projection>
{
    if (root == nullptr)
    {
        throw std::invalid_argument("bad root, null");
    }

    bool has_lhs    = root->lhs != nullptr;
    bool has_rhs    = root->rhs != nullptr;

    if (has_lhs ^ has_rhs == true)
    {
        throw std::invalid_argument("bad root, not complete tree");
    }

    bool is_childless   = root->lhs == nullptr;

    std::vector<Projection> rs{};

    if (is_childless)
    {
        for (size_t i = 0u; i < root->semantic_vec.size(); ++i)
        {
            rs.push_back
            (
                Projection
                {
                    .x  = std::vector<float>{root->semantic_vec[i]},
                    .y  = root->semantic_vec[i]
                }
            );
        }

        return rs;
    }

    if (root->semantic_vec.empty())
    {
        throw std::invalid_argument("bad root, empty semantic vector");
    }

    std::unordered_map<float, std::unordered_map<float, float>> semantic_map{};
    size_t semantic_idx = 0u;

    for (float lhs_semantic: root->lhs->semantic_vec)
    {
        for (float rhs_semantic: root->rhs->semantic_vec)
        {
            semantic_map[lhs_semantic][rhs_semantic] = root->semantic_vec[semantic_idx % root->semantic_vec.size()];
            semantic_idx    += 1;
        }
    }

    std::vector<Projection> lhs_projection_vec  = make_projection(root->lhs);
    std::vector<Projection> rhs_projection_vec  = make_projection(root->rhs);

    for (const Projection& lhs_projection: lhs_projection_vec)
    {
        for (const Projection& rhs_projection: rhs_projection_vec)
        {
            std::vector<float> x    = lhs_projection.x;
            x.insert(x.end(), rhs_projection.x.begin(), rhs_projection.x.end());
            float y                 = semantic_map.at(lhs_projection.y).at(rhs_projection.y);

            rs.push_back
            (
                Projection
                {
                    .x  = std::move(x),
                    .y  = y
                }
            );
        }
    }

    return rs;
}

auto stringify_projection(const Projection& projection) -> std::string
{
    std::string rs  = {};
    rs              += "input > ";

    for (float x: projection.x)
    {
        rs += std::to_string(x) + ", ";
    }

    rs              += "\n";
    rs              += "output > ";
    rs              += std::to_string(projection.y);

    return rs;
}

struct insufficient_coefficient_size: std::invalid_argument
{
    insufficient_coefficient_size(): std::invalid_argument("insufficient coefficient size"){}
};

auto two_dimensional_interpolated_project(float x0, float x1,
                                          size_t x0_slot,
                                          size_t x1_slot,
                                          size_t discretization_sz,
                                          const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap) -> float
{
    using namespace taylor_matrix::host_matrix::taylor_projection;

    if (x0_slot >= discretization_sz)
    {
        throw std::invalid_argument("bad x0 slot, >= discretization size");
    }

    if (x1_slot >= discretization_sz)
    {
        throw std::invalid_argument("bad x1 slot, >= discretization size");
    }

    size_t total_slot_sz        = discretization_sz * discretization_sz;
    size_t coeff_per_slot_sz    = get_multivariate_taylor_projection_coefficient_size(2u, 2u);
    size_t required_sz          = coeff_per_slot_sz * total_slot_sz;
    size_t next_offset          = coeff_arr_offset + required_sz;

    if (next_offset > coeff_arr_cap)
    {
        throw insufficient_coefficient_size();
    }

    size_t proj_slot    = x0_slot * discretization_sz + x1_slot;
    size_t proj_offset  = coeff_arr_offset + proj_slot * coeff_per_slot_sz;

    float x_arr[]{x0, x1};

    float rs            = multivariate_taylor_project(x_arr, stdx::to_size_container(std::integral_constant<size_t, 2u>{}),
                                                      stdx::to_size_container(std::integral_constant<size_t, 2u>{}),
                                                      coeff_arr, proj_offset, coeff_arr_cap);

    coeff_arr_offset    = next_offset;

    return rs;
}

auto binary_unf_interpolated_project(const float * x_arr, size_t x_arr_sz,
                                     float x_first, float x_last, size_t discretization_sz,
                                     const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap) -> float
{
    if (x_arr_sz == 0u)
    {
        throw std::invalid_argument("bad x_arr_sz, 0");
    }

    //right, this should be lhs =, rhs = but we'd cut some slack here, it's semantically different

    if (x_arr_sz == 1u)
    {
        return x_arr[0];
    }

    if (std::isnan(x_first))
    {
        throw std::invalid_argument("bad x_first, NaN");
    }

    if (std::isnan(x_last))
    {
        throw std::invalid_argument("bad x_last, NaN");
    }

    if (x_first >= x_last)
    {
        throw std::invalid_argument("bad interval x_first >= x_last");
    }

    if (x_arr_sz % 2u != 0u)
    {
        throw std::invalid_argument("bad x_arr_sz, not multiples of 2");
    }

    if (discretization_sz == 0u)
    {
        throw std::invalid_argument("bad discretization size, 0");
    }

    float global_interval           = x_last - x_first;
    float discretization_interval   = global_interval / discretization_sz;

    float lhs                       = binary_unf_interpolated_project(x_arr, x_arr_sz / 2,
                                                                      x_first, x_last, discretization_sz,
                                                                      coeff_arr, coeff_arr_offset, coeff_arr_cap);

    float rhs                       = binary_unf_interpolated_project(std::next(x_arr, x_arr_sz / 2), x_arr_sz / 2,
                                                                      x_first, x_last, discretization_sz,
                                                                      coeff_arr, coeff_arr_offset, coeff_arr_cap);

    if (std::isnan(lhs))
    {
        return lhs;
    }

    float _lhs                      = std::clamp(lhs, x_first, x_last);
    size_t tentative_lhs_slot       = (_lhs - x_first) / discretization_interval;
    size_t lhs_slot                 = std::min(tentative_lhs_slot, static_cast<size_t>(discretization_sz - 1u));

    if (std::isnan(rhs))
    {
        return rhs;
    }

    float _rhs                      = std::clamp(rhs, x_first, x_last);
    size_t tentative_rhs_slot       = (_rhs - x_first) / discretization_interval;
    size_t rhs_slot                 = std::min(tentative_rhs_slot, static_cast<size_t>(discretization_sz - 1u));

    return two_dimensional_interpolated_project(lhs, rhs,
                                                lhs_slot,
                                                rhs_slot,
                                                discretization_sz,
                                                coeff_arr, coeff_arr_offset, coeff_arr_cap);
}

auto get_binary_unf_interpolated_projection_size(size_t x_arr_sz,
                                                 float x_first, float x_last, float discretization_sz)
{
    std::vector<float> x_vec(x_arr_sz, 0.f);
    size_t cur_cap   = 1;

    while (true)
    {
        size_t cur_sz   = 0u;
        std::vector<float> coeff_vec(cur_cap, 0.f);

        try
        {
            binary_unf_interpolated_project(x_vec.data(), x_arr_sz,
                                            x_first, x_last, discretization_sz,
                                            coeff_vec.data(), cur_sz, cur_cap);

            return cur_sz;
        }
        catch (const insufficient_coefficient_size& e)
        {
            cur_cap *= 2;
        }
    }
}

auto get_optimizer() -> std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
{
    return std::make_unique<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
    (
        matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngineConfig
        {
            .matrix_cache_map_cap                       = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .time_machine_cache_map_cap                 = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .optimization_epoch_sz                      = 128ULL,
            .optimization_step_sz                       = 4ULL,
            .optimization_loop_sz                       = 2ULL
        }
    );
}

static inline const std::unordered_map<size_t, std::vector<size_t>> SHAPE_MAP = 
{
    {
        32,
        {
            size_t{1} << 1,
            1,
            tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
            tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
        }
    },
    {
        128,
        {
            size_t{1} << 2,
            2,
            tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
            tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
        }
    },
    {
        1024,
        {
            size_t{1} << 4,
            4,
            tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
            tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
        }
    }
};

auto get_matrix(const std::vector<float>& arg) -> std::shared_ptr<tensor_model::Matrix>
{
    std::vector<tensor_std_float_t> tensor_vec{};

    for (float e: arg)
    {
        tensor_vec.push_back(static_cast<tensor_std_float_t>(e));
    }

    return make_matrix_from_flat_vec(SHAPE_MAP.at(arg.size()), tensor_vec);
}

class UniformDiscretizer
{
    private:

        float first;
        float last;
        size_t discretization_sz;

    public:

        UniformDiscretizer(float first_arg,
                           float last_arg,
                           size_t discretization_sz_arg)
        {
            if (std::isnan(first_arg))
            {
                throw std::invalid_argument("bad first, NaN");
            }

            if (std::isnan(last_arg))
            {
                throw std::invalid_argument("bad last, NaN");
            }

            if (first_arg >= last_arg)
            {
                throw std::invalid_argument("bad [first, last), first >= last");
            }

            if (discretization_sz_arg == 0u)
            {
                throw std::invalid_argument("bad discretization size, 0");
            }

            this->first             = first_arg;
            this->last              = last_arg;
            this->discretization_sz = discretization_sz_arg;
        }

        auto discretize(float x) const -> size_t
        {
            float interval          = (last - first) / discretization_sz;
            intmax_t tentative_slot = (x - this->first) / interval;

            return std::clamp(tentative_slot,
                              intmax_t{0},
                              static_cast<intmax_t>(this->discretization_sz - 1u));

        }

        auto discretization_size() const noexcept -> size_t
        {
            return this->discretization_sz;
        }
};

auto get_training_pair(const std::vector<float>& x,
                       float y,
                       const UniformDiscretizer& discretizer) -> std::pair<std::shared_ptr<tensor_model::Matrix>,
                                                                           std::shared_ptr<tensor_model::Matrix>>
{
    const size_t MULTIPLIER_FACTOR  = tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;
    std::vector<float> inflated_x   = {};

    for (float e: x)
    {
        size_t e_slot       = inflated_x.size() + MULTIPLIER_FACTOR  / 2;

        for (size_t i = 0u; i < MULTIPLIER_FACTOR; ++i)
        {
            inflated_x.push_back(0);
        }

        inflated_x[e_slot]  = e;
    }

    std::optional<size_t> operable_sz   = std::nullopt;

    for (const auto& [sz, shape]: SHAPE_MAP)
    {
        if (sz > inflated_x.size())
        {
            operable_sz = sz;
            break;
        }
    }

    if (!operable_sz.has_value())
    {
        throw std::invalid_argument("bad operable size, no matrix shape found");
    }

    if (operable_sz.value() % inflated_x.size() != 0u)
    {
        throw std::invalid_argument("bad operable size, not multiplies of x_sz * MULTIPLIER_FACTOR"); //weird
    }

    size_t multiplication_factor        = operable_sz.value() / inflated_x.size();
    std::vector<float> flat_inp_matrix  = {};

    for (size_t i = 0u; i < multiplication_factor; ++i)
    {
        flat_inp_matrix.insert(flat_inp_matrix.end(), inflated_x.begin(), inflated_x.end());
    }

    if (operable_sz.value() % discretizer.discretization_size() != 0u)
    {
        throw std::invalid_argument("bad discretization size, not divisible by operable size");
    }

    std::vector<float> flat_out_matrix  = {};
    size_t popcount_per_punch_slot      = operable_sz.value() / discretizer.discretization_size();
    size_t out_punch_slot               = discretizer.discretize(y);

    for (size_t i = 0u; i < discretizer.discretization_size(); ++i)
    {
        for (size_t j = 0u; j < popcount_per_punch_slot; ++j)
        {
            if (i == out_punch_slot)
            {
                flat_out_matrix.push_back(1);
            }
            else
            {
                flat_out_matrix.push_back(0);
            }
        }
    }

    return std::make_pair
    (
        make_matrix_from_flat_vec(SHAPE_MAP.at(operable_sz.value()), std::vector<tensor_std_float_t>(stdx::to_castable_vector_initializer(flat_inp_matrix))),
        make_matrix_from_flat_vec(SHAPE_MAP.at(operable_sz.value()), std::vector<tensor_std_float_t>(stdx::to_castable_vector_initializer(flat_out_matrix)))
    );
}

class TheHostMatrixFactory2
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

        using self = TheHostMatrixFactory2;

        uint8_t compute_option;
        uint8_t entropy_option;

        std::optional<size_t> vector_sz;
        std::optional<size_t> base_transformation_sz;

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
            {2},
            {2, 2},
            {2, 2, 2},
            {2, 2, 2, 2}
        };

        static inline const std::vector<std::vector<size_t>> MID_TRANSFORMATION_ROTATION_VEC = 
        {
            {},
            {2},
            {2, 2},
            {2, 2, 2},
            {2, 2, 2, 2}
        };

        static inline const std::vector<std::vector<size_t>> HIGH_TRANSFORMATION_ROTATION_VEC = 
        {
            {},
            {2},
            {2, 2},
            {2, 2, 2},
            {2, 2, 2, 2}
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

                    DispatchCodeGenerator generator(make_matrix_from_shape_vec(matrix_shape),
                                                    hash_table_sz);

                    QuantizationMachine<PromotedFloatType> quant_machine(quant_data.discretization_sz,
                                                                            quant_data.exp_base,
                                                                            quant_data.multiplier_base);

                    matrix_transform(make_matrix_from_shape_vec(matrix_shape),
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

        TheHostMatrixFactory2(): compute_option(LOW_COMPUTE),
                                 entropy_option(LOW_ENTROPY),
                                 vector_sz(std::nullopt),
                                 base_transformation_sz(std::nullopt){}

        auto set_entropy(uint8_t entropy_option) -> TheHostMatrixFactory2&
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

        auto set_compute(uint8_t compute_option) -> TheHostMatrixFactory2&
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

        auto set_vector_size(size_t sz) -> TheHostMatrixFactory2&
        {
            this->vector_sz = sz;

            return *this;
        }

        auto compute() -> TheHostMatrixFactory2&
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

auto get_parity_distance(const std::shared_ptr<tensor_model::Matrix>& lhs,
                         const std::shared_ptr<tensor_model::Matrix>& rhs) -> double
{
    std::vector<tensor_std_float_t> lhs_flat_tensor_vec{};
    std::vector<tensor_std_float_t> rhs_flat_tensor_vec{};

    tensor_factory::flatten(lhs, lhs_flat_tensor_vec);
    tensor_factory::flatten(rhs, rhs_flat_tensor_vec);

    if (lhs_flat_tensor_vec.size() != rhs_flat_tensor_vec.size())
    {
        std::cout << "mayday, mismatched tensor logit vector\n";
        std::abort();
    }

    double one_parity_score = 0;
    size_t parity_sz        = 0u;

    for (size_t i = 0u; i < rhs_flat_tensor_vec.size(); ++i)
    {
        if (rhs_flat_tensor_vec[i] == 1)
        {
            parity_sz           += 1;
            one_parity_score    = std::max(one_parity_score, static_cast<double>(std::exp(lhs_flat_tensor_vec[i])));
        }
    }

    size_t parity_hub_sz            = rhs_flat_tensor_vec.size() / parity_sz;
    double max_round_parity_score   = 0;

    for (size_t i = 0u; i < parity_hub_sz; ++i)
    {
        double round_parity_score   = 0;

        for (size_t j = 0u; j < parity_sz; ++j)
        {
            size_t idx  = i * parity_sz + j;
            
            if (rhs_flat_tensor_vec[idx] == 1)
            {
                continue;
            }

            round_parity_score  = std::max(round_parity_score, static_cast<double>(std::exp(lhs_flat_tensor_vec[idx])));
        }

        max_round_parity_score  = std::max(round_parity_score, max_round_parity_score);
    }

    if (one_parity_score > max_round_parity_score)
    {
        return 0;
    }

    double lhs_parity   = one_parity_score - max_round_parity_score;
    double rhs_parity   = parity_sz;

    return std::pow(lhs_parity - rhs_parity, 2); 
}

auto is_same_parity(const std::shared_ptr<tensor_model::Matrix>& lhs,
                    const std::shared_ptr<tensor_model::Matrix>& rhs) -> bool
{
    std::vector<tensor_std_float_t> lhs_flat_tensor_vec{};
    std::vector<tensor_std_float_t> rhs_flat_tensor_vec{};

    tensor_factory::flatten(lhs, lhs_flat_tensor_vec);
    tensor_factory::flatten(rhs, rhs_flat_tensor_vec);

    //what we'd want is not parity distance, in this particular scenerio

    if (lhs_flat_tensor_vec.size() != rhs_flat_tensor_vec.size())
    {
        std::cout << "mayday, mismatched tensor logit vector\n";
        std::abort();
    }

    double one_parity_score = 0;
    size_t parity_sz        = 0u;

    for (size_t i = 0u; i < rhs_flat_tensor_vec.size(); ++i)
    {
        if (rhs_flat_tensor_vec[i] == 1)
        {
            parity_sz           += 1;
            one_parity_score    += std::exp(lhs_flat_tensor_vec[i]);
        }
    }

    size_t parity_hub_sz            = rhs_flat_tensor_vec.size() / parity_sz;
    double max_round_parity_score   = 0;

    for (size_t i = 0u; i < parity_hub_sz; ++i)
    {
        double round_parity_score   = 0;

        for (size_t j = 0u; j < parity_sz; ++j)
        {
            size_t idx  = i * parity_sz + j;
            
            if (rhs_flat_tensor_vec[idx] == 1)
            {
                continue;
            }

            round_parity_score  += std::exp(lhs_flat_tensor_vec[idx]);
        }

        max_round_parity_score  = std::max(round_parity_score, max_round_parity_score);
    }

    return one_parity_score > max_round_parity_score;
}

class PointPullMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> training_pair_vec;

    public:

        PointPullMatrixEvaluator(std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> training_pair_vec): training_pair_vec(std::move(training_pair_vec)){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<std::shared_ptr<tensor_model::Matrix>> inp_vec      = {};
            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec_0    = {};

            for (const auto& [inp, out]: this->training_pair_vec)
            {
                inp_vec.push_back(inp);
                out_vec_0.push_back(out);
            }

            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec      = matrix.project(inp_vec);
            double rs = 0;

            for (const auto [out, expected_out]: stdx::zip(out_vec, out_vec_0))
            {
                rs += get_parity_distance(out, expected_out);
            }

            return rs / std::pow(get_score(matrix), 2);
        }

        auto get_unscaled_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<std::shared_ptr<tensor_model::Matrix>> inp_vec      = {};
            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec_0    = {};

            for (const auto& [inp, out]: this->training_pair_vec)
            {
                inp_vec.push_back(inp);
                out_vec_0.push_back(out);
            }

            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec      = matrix.project(inp_vec);
            double rs = 0;

            for (const auto [out, expected_out]: stdx::zip(out_vec, out_vec_0))
            {
                rs += get_parity_distance(out, expected_out);
            }

            return rs;
        }

        auto get_score(the_matrix::MatrixInterface& matrix) -> double
        {
            std::vector<std::shared_ptr<tensor_model::Matrix>> inp_vec      = {};
            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec_0    = {};

            for (const auto& [inp, out]: this->training_pair_vec)
            {
                inp_vec.push_back(inp);
                out_vec_0.push_back(out);
            }

            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec      = matrix.project(inp_vec);
            double hit      = 0;
            double total    = 0;

            for (const auto& [out, expected_out]: stdx::zip(out_vec, out_vec_0))
            {
                hit     += is_same_parity(out, expected_out);
                total   += 1;
            }

            if (total == 0)
            {
                return 0;
            }

            return hit / total;
        }
};

void run_test()
{
    const size_t HEIGHT                 = 4;
    const float DISCRETIZATION_VALUE    = 0.2;
    const size_t SEMANTIC_SZ            = 2;
    const size_t EPOCH_SZ               = size_t{1} << 8;

    UniformDiscretizer discretizer(0.f,
                                   DISCRETIZATION_VALUE * SEMANTIC_SZ,
                                   SEMANTIC_SZ);

    std::shared_ptr<NodeContainer> node_container                                                                               = make_node_container(make_tree(HEIGHT, DISCRETIZATION_VALUE, SEMANTIC_SZ));
    std::vector<Projection> projection_vec                                                                                      = make_projection(node_container->root);

    std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> optimizer                                     = get_optimizer();
    std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> projection_pair_vec    = {};
    
    for (const auto& projection: projection_vec)
    {
        auto [inp, out] = get_training_pair(projection.x, projection.y, discretizer);
        projection_pair_vec.push_back
        (
            std::make_pair
            (
                inp,
                out
            )
        );
    }

    if (projection_pair_vec.size() == 0u)
    {
        std::abort();
    }

    std::vector<tensor_std_float_t> flat_inp_vec{};
    tensor_factory::flatten(projection_pair_vec.front().first, flat_inp_vec);
    size_t flat_inp_vec_sz  = flat_inp_vec.size();

    std::cout << "projection vector size > " << projection_vec.size() << "\n";
    std::cout << "projection.x vector size > " << projection_vec.front().x.size() << "\n";

    std::shared_ptr<the_matrix::MatrixInterface> matrix                                     = TheHostMatrixFactory2{}.set_vector_size(flat_inp_vec_sz).get();
    std::unique_ptr<PointPullMatrixEvaluator> matrix_evaluator                              = std::make_unique<PointPullMatrixEvaluator>(projection_pair_vec);
    common_exception::CancellationToken cancellation_token                                  = {};

    {
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);

        std::cout << "i > " << -1 << " deviation > " << optimized_deviation << "\n";
    }

    for (size_t i = 0u; i < EPOCH_SZ; ++i)
    {
        matrix                      = optimizer->optimize(*matrix, *matrix_evaluator, cancellation_token);
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);

        std::cout << "i > " << i << " deviation > " << optimized_deviation << "\n";
    }
}


void initialize_concurrency_base()
{
    using namespace concurrency_base;

    std::cout << "initializing concurrency base\n";
    std::vector<WorkerInformation> worker_info_vec{};

    for (size_t i = 0u; i < 8u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation
        {
            .cpu_id = std::nullopt,
            .daemon = ASYNC_SEQPAR_DAEMON
        });
    }

    init(Config{worker_info_vec});
    async_x::init(8u, 32u);
}

int main()
{
    initialize_concurrency_base();
    run_test();
}