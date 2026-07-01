#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <matrix_optimizer_subsystem/config_builder.h>
#include <matrix_optimizer_subsystem/coordinated_search_optimizer_engine.h>
#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <random>
#include <functional>
#include <algorithm>
#include <iostream>
#include <memory>
#include <matrix_steering_subsystem/taylor_projection.h>
#include <matrix_steering_subsystem/shape_projection.h>
#include <matrix_steering_subsystem/by_step_optimizer.h>
#include <matrix/tensor_model.h>
#include <general_definition/float_def.h>
#include <math.h>
#include <type_traits>
#include <stl_extension/stdx.h>
#include <stl_extension/hasher.h>
#include <numeric>
#include <limits.h>
#include <stdexcept>
#include <exception>
#include <utility>
#include <vector>

using tensor_std_float_t = tensor_model::tensor_std_float_t;

using namespace float_def;

auto randomize_int(size_t first, size_t last) -> size_t
{
    if (first >= last)
    {
        std::abort();
    }

    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return first + randomizer() % (last - first);
}

template <class T>
auto randomize_optional_int(size_t first, size_t last) -> std::optional<T>
{
    static_assert(std::is_unsigned_v<T>);

    if (randomize_int(0, 1) == 0)
    {
        return std::nullopt;
    }

    return randomize_int(first, last);
}

auto randomize_double(double first, double last) -> double
{
    if (first >= last)
    {
        std::abort();
    }

    static auto distributor = std::uniform_real_distribution<double>(first, last);
    static auto randomizer  = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};

    return distributor(randomizer);
}

auto get_random_point_bag(size_t sz) -> std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>>
{
    std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> rs{};

    for (size_t i = 0u; i < sz; ++i)
    {
        rs.push_back(std::make_pair(randomize_double(0, 2), randomize_double(0, 2)));
    }

    return rs;
}

class ExponentialQuantizationMachine
{
    private:

        std::vector<std::pair<float, float>> quantization_range_vec;

        static inline constexpr float MULTIPLIER_BASE_MIN   = 0;
        static inline constexpr float MULTIPLIER_BASE_MAX   = 10;

        static inline constexpr float EXP_BASE_MIN          = 1;
        static inline constexpr float EXP_BASE_MAX          = 10;

    public:

        ExponentialQuantizationMachine(float exp_base,
                                       float multiplier_base,
                                       size_t quantization_sz): quantization_range_vec()
        {
            if (quantization_sz == 0u)
            {
                throw std::invalid_argument("bad quantization size, 0");
            }

            if (std::isnan(exp_base))
            {
                throw std::invalid_argument("bad exp base, NaN");
            }

            if (std::clamp(exp_base, EXP_BASE_MIN, EXP_BASE_MAX) != exp_base)
            {
                throw std::invalid_argument("bad exp base, not in [0, 10] range");
            }

            if (std::isnan(multiplier_base))
            {
                throw std::invalid_argument("bad multiplier base, NaN");
            }

            if (std::clamp(multiplier_base, MULTIPLIER_BASE_MIN, MULTIPLIER_BASE_MAX) != multiplier_base)
            {
                throw std::invalid_argument("bad multiplier base, not in [0, 10] range");
            }

            std::vector<std::pair<float, float>> first_last_tmp_vec = {};
            float first = 0;

            for (size_t i = 0u; i < quantization_sz; ++i)
            {
                float last  = multiplier_base * std::pow(exp_base, i);
                first_last_tmp_vec.push_back(std::make_pair(first, last));
                first       = last;
            }

            for (size_t i = 0u; i < quantization_sz; ++i)
            {
                size_t rev_i        = quantization_sz - i - 1;
                float local_first   = first_last_tmp_vec[rev_i].second;
                float local_last    = first_last_tmp_vec[rev_i].first;

                if (i == 0u)
                {
                    local_first = -std::numeric_limits<float>::infinity();
                }

                this->quantization_range_vec.push_back(std::make_pair(local_first, local_last));
            }

            for (size_t i = 0u; i < quantization_sz; ++i)
            {
                float local_first   = first_last_tmp_vec[i].first;
                float local_last    = first_last_tmp_vec[i].second;

                if (i + 1 == quantization_sz)
                {
                    local_last  = std::numeric_limits<float>::infinity();
                }

                this->quantization_range_vec.push_back(std::make_pair(local_first, local_last));
            }
        }

        auto get_quantization_bucket(float x) const -> size_t
        {
            if (std::isnan(x))
            {
                throw std::invalid_argument("bad x, NaN");
            }

            auto bucket_ptr = std::lower_bound(this->quantization_range_vec.begin(),
                                               this->quantization_range_vec.end(),
                                               x,
                                               [](const auto& e, float x)
                                                {
                                                    return e.second <= x;
                                                });
            
            if (this->quantization_range_vec.size() == 0u)
            {
                std::abort();
            }

            size_t ptr  = std::min(static_cast<size_t>(std::distance(quantization_range_vec.begin(), bucket_ptr)),
                                   static_cast<size_t>(quantization_range_vec.size() - 1));

            return ptr;
        }

        auto get_boundary(size_t i) const -> std::pair<float, float>
        {
            if (i >= this->quantization_range_vec.size())
            {
                throw std::invalid_argument("bad bucket index, out of range [0, sz)");
            }

            return this->quantization_range_vec[i];
        }

        auto size() const noexcept -> size_t
        {
            return this->quantization_range_vec.size();
        }
};

class Projector
{
    private:

        std::vector<float> coeff_vec;
    
    public:

        Projector(std::vector<float> coeff_vec): coeff_vec(std::move(coeff_vec)){}

        auto project(float x)
        {
            return taylor_project(x,
                                  coeff_vec.data(), stdx::to_size_container(coeff_vec.size()));
        }

        void set_coefficient_vector(const std::vector<float>& arg)
        {
            if (arg.size() != this->coeff_vec.size())
            {
                throw std::invalid_argument("bad coefficient size, mismatched size");
            }

            this->coeff_vec = arg;
        }

        auto get_coefficient_vector() const noexcept -> const std::vector<float>&
        {
            return this->coeff_vec;
        }
    
    private:

        template <class FloatType, class SzContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
        auto taylor_project(FloatType x,
                            const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                            const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                            const std::integral_constant<bool, HasBoundCheck>& bound_check = std::integral_constant<bool, HasBoundCheck>{}) -> PromotedFloatType
        {
            static_assert(std::is_floating_point_v<FloatType>);
            static_assert(std::is_floating_point_v<PromotedFloatType>);

            PromotedFloatType projected_result  = 0;
            PromotedFloatType x_multiplier      = 1;
            size_t factorial_denum              = 1;

            for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
            {
                PromotedFloatType delta_result  = static_cast<PromotedFloatType>(coeff_arr[i]) / static_cast<PromotedFloatType>(factorial_denum) * x_multiplier;
                projected_result                += delta_result;
                x_multiplier                    *= x;
                factorial_denum                 *= i + 1;
            }

            return projected_result;
        }
};

class SplineProjector
{
    private:

        std::vector<std::unique_ptr<Projector>> projector_vec;
    
    public:

        SplineProjector(std::vector<std::vector<float>> coeff_2d_vec): projector_vec()
        {
            for (auto& coeff_vec: coeff_2d_vec)
            {
                this->projector_vec.push_back(std::make_unique<Projector>(std::move(coeff_vec)));
            }
        }

        auto project(float x, size_t idx) -> float //I know that I should encapsulate whatever, but this is for ease of ...
        {
            if (idx >= this->projector_vec.size())
            {
                throw std::invalid_argument("bad index, out of range [0, sz)"); // should be vector problem
            }

            return this->projector_vec[idx]->project(x);
        }

        auto size() const noexcept -> size_t
        {
            return this->projector_vec.size();
        }

        void set_coefficient_vector(const std::vector<std::vector<float>>& coeff_2d_vec)
        {
            if (coeff_2d_vec.size() != this->projector_vec.size())
            {
                throw std::invalid_argument("bad coefficient 2d vector, mismatched size");
            }

            for (size_t i = 0u; i < this->projector_vec.size(); ++i)
            {
                try
                {
                    this->projector_vec[i]->set_coefficient_vector(coeff_2d_vec[i]);
                }
                catch (...)
                {
                    std::abort();
                }
            }
        }

        auto get_coefficient_vector() const -> std::vector<std::vector<float>>
        {
            std::vector<std::vector<float>> rs{};

            for (const auto& projector: this->projector_vec)
            {
                rs.push_back(projector->get_coefficient_vector());
            }

            return rs;
        }
};

struct AnchorPoint
{
    float x;
    float y;
    size_t spline_idx;
};

auto get_anchor_point_set(const std::shared_ptr<SplineProjector>& spline_projector,
                          const std::shared_ptr<ExponentialQuantizationMachine>& quant_machine) -> std::vector<AnchorPoint>
{
    constexpr float SPLINE_JOIN_BOUNDARY_ANCHOR_DX = 0.01;

    std::vector<AnchorPoint> rs{};

    for (size_t i = 1u; i < quant_machine->size(); ++i)
    {
        float prev_x_first  = quant_machine->get_boundary(i - 1).first;
        float prev_x_last   = quant_machine->get_boundary(i - 1).second;

        float x_0           = prev_x_last;
        float y_0           = spline_projector->project(x_0, i - 1);

        float x_1           = prev_x_last - SPLINE_JOIN_BOUNDARY_ANCHOR_DX;
        float y_1           = spline_projector->project(x_1, i - 1);

        rs.push_back
        (
            AnchorPoint
            {
                .x          = x_0,
                .y          = y_0,
                .spline_idx = i - 1
            }
        );

        rs.push_back
        (
            AnchorPoint
            {
                .x          = x_1,
                .y          = y_1,
                .spline_idx = i - 1
            }
        );
    }

    return rs;
}

template <class T>
auto split_vector(const std::vector<T>& vec, size_t split_sz) -> std::vector<std::vector<T>>
{
    if (split_sz == 0u)
    {
        throw std::invalid_argument("bad split size, 0");
    }

    if (vec.size() % split_sz != 0u)
    {
        throw std::invalid_argument("bad vector size, not multiplies of split_sz");
    }

    size_t chunk_sz = vec.size() / split_sz;
    std::vector<std::vector<T>> rs{};

    for (size_t i = 0u; i < split_sz; ++i)
    {
        size_t first    = chunk_sz * i;
        size_t last     = chunk_sz * (i + 1);

        rs.push_back
        (
            std::vector<T>
            (
                std::next(vec.begin(), first),
                std::next(vec.begin(), last)
            )
        );
    }

    return rs;
}

class CubicSplinePointPullMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::shared_ptr<SplineProjector> spline_projector;
        std::shared_ptr<ExponentialQuantizationMachine> quant_machine;
        std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_vec;

    public:

        CubicSplinePointPullMatrixEvaluator(std::shared_ptr<SplineProjector> spline_projector,
                                            std::shared_ptr<ExponentialQuantizationMachine> quant_machine,
                                            std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_vec): spline_projector(std::move(spline_projector)),
                                                                                                                       quant_machine(std::move(quant_machine)),
                                                                                                                       point_vec(std::move(point_vec)){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> expected_vec{};
            std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> projected_vec{};

            auto coeff_vec = matrix.get_coefficient_vector();
            this->spline_projector->set_coefficient_vector(split_vector(coeff_vec, this->spline_projector->size()));

            for (const auto& [x, y]: this->point_vec)
            {
                size_t idx      = this->quant_machine->get_quantization_bucket(x);
                float actual_y  = this->spline_projector->project(x, idx);

                expected_vec.push_back(std::make_pair(x, y));
                projected_vec.push_back(std::make_pair(x, actual_y));
            }

            for (const AnchorPoint& anchor_point: get_anchor_point_set(this->spline_projector, this->quant_machine))
            {
                float actual_y  = this->spline_projector->project(anchor_point.x, anchor_point.spline_idx);

                expected_vec.push_back(std::make_pair(anchor_point.x, anchor_point.y));
                projected_vec.push_back(std::make_pair(anchor_point.x, actual_y));
            }

            return this->mean_square_root(projected_vec, expected_vec);
        }

    private:

        auto mean_square_root(const std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>>& lhs,
                              const std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>>& rhs) -> eval_float_t
        {
            if (lhs.size() != rhs.size())
            {
                std::cout << "mayday, mismatched tensor dimension size\n";
                std::abort();
            }

            eval_float_t rs = 0;

            for (size_t i = 0u; i < lhs.size(); ++i)
            {
                rs += std::pow(lhs[i].first - rhs[i].first, 2);
                rs += std::pow(lhs[i].second - rhs[i].second, 2);
            }

            if (lhs.size() != 0u)
            {
                rs /= lhs.size() * 2;
            }

            return std::sqrt(rs);
        }
};

class SplineProjectorMatrixWrapper: public virtual the_matrix::MatrixInterface
{
    private:

        std::shared_ptr<SplineProjector> spline_projector;

    public:

        SplineProjectorMatrixWrapper(std::shared_ptr<SplineProjector> spline_projector): spline_projector(std::move(spline_projector))
        {
            if (this->spline_projector == nullptr)
            {
                throw std::invalid_argument("bad spline projector, null");
            }
        }

        auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
        {
            std::vector<tensor_std_float_t> rs{};

            for (const auto& e: this->spline_projector->get_coefficient_vector())
            {
                rs.insert(rs.end(), e.begin(), e.end());
            }

            return rs;
        }

        void set_coefficient_vector(const std::vector<tensor_std_float_t>& coeff_vec)
        {
            std::vector<std::vector<tensor_std_float_t>> coeff_2d_vec   = split_vector(coeff_vec, this->spline_projector->size());

            this->spline_projector->set_coefficient_vector(coeff_2d_vec);
        }

        auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
        {
            throw std::invalid_argument("bad invoke, project function disabled");
        }

        auto clone() -> std::shared_ptr<the_matrix::MatrixInterface>
        {
            return std::make_shared<SplineProjectorMatrixWrapper>(std::make_shared<SplineProjector>(this->spline_projector->get_coefficient_vector()));
        }
};

auto get_quantization_machine() -> std::unique_ptr<ExponentialQuantizationMachine>
{
    const float EXP_BASE            = 1.2;
    const float MULTIPLIER_BASE     = 0.1;
    const size_t QUANTIZATION_SZ    = 30;

    return std::make_unique<ExponentialQuantizationMachine>(EXP_BASE,
                                                            MULTIPLIER_BASE,
                                                            QUANTIZATION_SZ);
}

auto get_uniform_spline_projector(size_t spline_sz,
                                  size_t coefficient_sz_per_spline) -> std::unique_ptr<SplineProjector>
{
    return std::make_unique<SplineProjector>(stdx::make_2d_vector<float>(spline_sz, coefficient_sz_per_spline));
}

auto get_matrix(const std::shared_ptr<SplineProjector>& spline_projector) -> std::unique_ptr<the_matrix::MatrixInterface>
{
    return std::make_unique<SplineProjectorMatrixWrapper>(spline_projector);
}

auto get_distributed_matrix_evaluator(const std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>>& point_vec,
                                      const std::shared_ptr<SplineProjector>& spline_projector,
                                      const std::shared_ptr<ExponentialQuantizationMachine>& quant_machine) -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
{
    return std::make_unique<CubicSplinePointPullMatrixEvaluator>(spline_projector, quant_machine, point_vec);
}

auto get_random_coordinated_search_optimizer_engine() -> std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
{
    return std::make_unique<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
    (
        matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngineConfig
        {
            .matrix_cache_map_cap       = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .time_machine_cache_map_cap = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .optimization_epoch_sz      = 512ULL,
            .optimization_step_sz       = 32LL,
            .optimization_loop_sz       = 4ULL
        }
    );
}

void run_one_test(const size_t point_pull_sz,
                  const size_t coefficient_sz,
                  const size_t hash_table_sz)
{
    const size_t OPTIMIZATION_SZ    = size_t{1} << 2;
    const size_t CUBIC_OVERHEAD     = 4u;

    std::shared_ptr<ExponentialQuantizationMachine> quant_machine                               = get_quantization_machine();
    std::shared_ptr<SplineProjector> spline_projector                                           = get_uniform_spline_projector(quant_machine->size(), CUBIC_OVERHEAD);
    std::shared_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> test_engine   = get_random_coordinated_search_optimizer_engine();

    std::shared_ptr<the_matrix::MatrixInterface> expected_matrix                                = get_matrix(spline_projector);

    std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_bag                    = get_random_point_bag(point_pull_sz);
    std::shared_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator                       = get_distributed_matrix_evaluator(point_bag, spline_projector, quant_machine);

    common_exception::CancellationToken cancellation_token{};

    std::cout << "__BEGIN_OPTIMIZATION_TEST__\n";

    std::cout << "point_pull_sz > " << point_pull_sz << "\n";
    std::cout << "coefficient_sz > " << coefficient_sz << "\n";

    double initial_deviation    = evaluator->get_deviation(*expected_matrix);
    std::cout << "initial deviation > " << initial_deviation << "\n";

    for (size_t i = 0u; i < OPTIMIZATION_SZ; ++i)
    {
        expected_matrix  = test_engine->optimize(*expected_matrix, *evaluator, cancellation_token);
        double optimized_deviation  = evaluator->get_deviation(*expected_matrix);

        std::cout << "optimized deviation > " << optimized_deviation << "\n";
    }

    std::cout << "__END_OPTIMIZATION_TEST__\n";
}


void run_test()
{
    // const std::vector<std::pair<size_t, size_t>> TEST_PAIR_VEC  = 
    // {
    //     {1, 2},
    //     {2, 2},
    //     {3, 2},
    //     {4, 2},
    //     {5, 2},
    //     {6, 2},
    //     {7, 2},
    //     {8, 2},
    //     {9, 2},
    //     {10, 2}
    // };

    const std::vector<std::tuple<size_t, size_t, size_t>> TEST_TUPLE_VEC  = 
    {
        // {2, 4, 4},
        {4, 4, 16},
        {8, 4, 32},
        {16, 4, 64},
        {32, 4, 128},
        {64, 4, 256},
        {128, 4, 512}
    };

    std::cout << "__BEGIN_COORDINATED_SEARCH_OPTIMIZER_ENGINE_POINT_PULL_TEST__\n";

    for (const auto& [point_pull_sz, coefficient_sz, hash_table_sz] : TEST_TUPLE_VEC)
    {
        std::cout << "-------------------\n";

        std::cout << "point_pull_sz > " << point_pull_sz << "\n";
        std::cout << "coefficient_sz > " << coefficient_sz << "\n";
        std::cout << "hash_table_sz > " << hash_table_sz << "\n";

        run_one_test(point_pull_sz, coefficient_sz, hash_table_sz);

        std::cout << "-------------------\n";
    }

    std::cout << "__END_COORDINATED_SEARCH_OPTIMIZER_ENGINE_POINT_PULL_TEST__\n";
}

int main()
{
    run_test();
}