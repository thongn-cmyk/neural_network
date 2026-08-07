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

class TaylorMatrix: public virtual the_matrix::MatrixInterface
{
    private:

        std::vector<tensor_std_float_t> tensor_vec;
    
    public:

        TaylorMatrix(std::vector<tensor_std_float_t> tensor_vec): tensor_vec(std::move(tensor_vec)){}

        auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
        {
            return this->tensor_vec;
        }

        void set_coefficient_vector(const std::vector<tensor_std_float_t>& coeff_vec)
        {
            this->tensor_vec = coeff_vec;
        }

        auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
        {
            return {};
        }

        auto clone() -> std::shared_ptr<the_matrix::MatrixInterface>
        {
            return std::make_shared<TaylorMatrix>(*this);
        }
};

template <class FloatType, class SzContainer, class PromotedFloatType = FloatType, bool HasBoundCheck = true>
auto project(FloatType x,
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

auto get_taylor_matrix(size_t coefficient_sz) -> std::unique_ptr<the_matrix::MatrixInterface>
{
    return std::make_unique<TaylorMatrix>(std::vector<tensor_std_float_t>(coefficient_sz, 0));
}

auto get_taylor_matrix_table(size_t coefficient_sz, size_t hash_sz) -> std::unique_ptr<the_matrix::MatrixInterface>
{
    return std::make_unique<TaylorMatrix>(std::vector<tensor_std_float_t>(coefficient_sz * hash_sz, 0));
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

class PointPullMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_vec;

    public:

        PointPullMatrixEvaluator(std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_vec): point_vec(std::move(point_vec)){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> projected_vec{};
            auto coeff_vec = matrix.get_coefficient_vector();

            for (const auto& [x, y]: this->point_vec)
            {
                projected_vec.push_back(std::make_pair(x, project(x,
                                                                  coeff_vec.data(), stdx::to_size_container(coeff_vec.size())) * coeff_vec.back()));
            }

            return this->mean_square_root(projected_vec, this->point_vec);
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

auto get_matrix_evaluator(const std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>>& point_vec) -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
{
    return std::make_unique<PointPullMatrixEvaluator>(point_vec);
}

auto get_random_coordinated_search_optimizer_engine() -> std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
{
    return std::make_unique<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
    (
        matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngineConfig
        {
            .matrix_cache_map_cap       = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .time_machine_cache_map_cap = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .optimization_epoch_sz      = 1ULL,
            .optimization_step_sz       = 131072ULL,
            .optimization_loop_sz       = 8ULL
        }
    );
}

void run_one_test(const size_t point_pull_sz,
                  const size_t coefficient_sz)
{
    const size_t OPTIMIZATION_SZ    = size_t{1} << 10;

    std::shared_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> test_engine   = get_random_coordinated_search_optimizer_engine();

    std::shared_ptr<the_matrix::MatrixInterface> expected_matrix                                = get_taylor_matrix(coefficient_sz);
    std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_bag                    = get_random_point_bag(point_pull_sz);
    std::shared_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator                       = get_matrix_evaluator(point_bag);

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

    const std::vector<std::pair<size_t, size_t>> TEST_PAIR_VEC  = 
    {
        // {1, 2},
        // {2, 3},
        // {3, 4},
        // {4, 5},
        // {5, 6},
        {6, 7},
        {7, 8},
        {8, 9},
        {9, 10},
        {10, 11}
    };

    std::cout << "__BEGIN_COORDINATED_SEARCH_OPTIMIZER_ENGINE_POINT_PULL_TEST__\n";

    for (const auto& [point_pull_sz, coefficient_sz] : TEST_PAIR_VEC)
    {
        std::cout << "-------------------\n";

        std::cout << "point_pull_sz > " << point_pull_sz << "\n";
        std::cout << "coefficient_sz > " << coefficient_sz << "\n";

        run_one_test(point_pull_sz, coefficient_sz);

        std::cout << "-------------------\n";
    }

    std::cout << "__END_COORDINATED_SEARCH_OPTIMIZER_ENGINE_POINT_PULL_TEST__\n";
}

int main()
{
    run_test();
}