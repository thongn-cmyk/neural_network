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
#include <taylor_matrix/host_matrix/shape_projection.h>

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
    return
    {
        {0,     1},
        {0.1,   0},
        {0.2,   1},
        {0.3,   0},
        {0.4,   1},
        {1,     0},
        {2,     1},
        {4,     0},
        {8,     1},
        {12,    0}
    };
}

class Projector
{
    private:

        std::vector<float> coeff_vec;
        size_t base_projection_sz;

    public:

        Projector(std::vector<float> coeff_vec,
                  size_t base_projection_sz): coeff_vec(std::move(coeff_vec)),
                                              base_projection_sz(base_projection_sz){}

        auto project(float x)
        {
            size_t sz   = 0u;
            size_t cap  = coeff_vec.size();

            return taylor_matrix::host_matrix::shape_projection::base_cubic_interpolated_taylor_raw_shape_project(x, stdx::to_size_container(this->base_projection_sz),
                                                                                                                  coeff_vec.data(), sz, cap);
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

        std::shared_ptr<Projector> spline_projector;
        std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_vec;

    public:

        CubicSplinePointPullMatrixEvaluator(std::shared_ptr<Projector> spline_projector,
                                            std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_vec): spline_projector(std::move(spline_projector)),
                                                                                                                       point_vec(std::move(point_vec)){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> expected_vec{};
            std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> projected_vec{};

            auto coeff_vec = matrix.get_coefficient_vector();
            this->spline_projector->set_coefficient_vector(coeff_vec);

            for (const auto& [x, y]: this->point_vec)
            {
                float actual_y  = this->spline_projector->project(x);

                expected_vec.push_back(std::make_pair(x, y));
                projected_vec.push_back(std::make_pair(x, actual_y));
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

        std::shared_ptr<Projector> spline_projector;

    public:

        SplineProjectorMatrixWrapper(std::shared_ptr<Projector> spline_projector): spline_projector(std::move(spline_projector))
        {
            if (this->spline_projector == nullptr)
            {
                throw std::invalid_argument("bad spline projector, null");
            }
        }

        auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
        {
            return spline_projector->get_coefficient_vector();
        }

        void set_coefficient_vector(const std::vector<tensor_std_float_t>& coeff_vec)
        {
            this->spline_projector->set_coefficient_vector(coeff_vec);
        }

        auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
        {
            throw std::invalid_argument("bad invoke, project function disabled");
        }

        auto clone() -> std::shared_ptr<the_matrix::MatrixInterface>
        {
            return std::make_shared<SplineProjectorMatrixWrapper>(std::make_shared<Projector>(*this->spline_projector));
        }
};

auto get_uniform_spline_projector(size_t coefficient_sz_per_spline) -> std::unique_ptr<Projector>
{
    return std::make_unique<Projector>(std::vector<float>(taylor_matrix::host_matrix::shape_projection::get_cubic_interpolated_taylor_raw_shape_projection_size(coefficient_sz_per_spline), 0),
                                       coefficient_sz_per_spline);
}

auto get_matrix(const std::shared_ptr<Projector>& spline_projector) -> std::unique_ptr<the_matrix::MatrixInterface>
{
    return std::make_unique<SplineProjectorMatrixWrapper>(spline_projector);
}

auto get_distributed_matrix_evaluator(const std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>>& point_vec,
                                      const std::shared_ptr<Projector>& spline_projector) -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
{
    return std::make_unique<CubicSplinePointPullMatrixEvaluator>(spline_projector, point_vec);
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

//if we can punch 90%, I will implement 2d uniform + exp
//                                      1d uniform + exp in separate files

void run_one_test(const size_t point_pull_sz,
                  const size_t coefficient_sz,
                  const size_t hash_table_sz)
{
    const size_t OPTIMIZATION_SZ    = size_t{1} << 4;
    const size_t CUBIC_OVERHEAD     = 4u;

    std::shared_ptr<Projector> spline_projector                                                 = get_uniform_spline_projector(CUBIC_OVERHEAD);
    std::shared_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> test_engine   = get_random_coordinated_search_optimizer_engine();

    std::shared_ptr<the_matrix::MatrixInterface> expected_matrix                                = get_matrix(spline_projector);

    std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_bag                    = get_random_point_bag(point_pull_sz);
    std::shared_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator                       = get_distributed_matrix_evaluator(point_bag, spline_projector);

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

//I will run the 2d interpolation, and 1d interpolation with the exponential hinge, I need to test the feasibility with static points first
//I think 2d interpolation will make a huge difference, let's see

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