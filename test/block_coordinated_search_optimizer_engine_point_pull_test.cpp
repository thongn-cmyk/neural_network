#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <matrix_optimizer_subsystem/config_builder.h>
#include <matrix_optimizer_subsystem/coordinated_search_optimizer_engine.h>
#include <matrix_optimizer_subsystem/generic_optimizer_engine.h>
#include <matrix_optimizer_subsystem/config_builder.h>

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
                projected_vec.push_back(std::make_pair(x, shape_projection::taylor_shape_project(x,
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

class HashTablePointPullMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_vec;
        size_t hash_sz;
    
    public:

        HashTablePointPullMatrixEvaluator(std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_vec,
                                          size_t hash_sz): point_vec(std::move(point_vec)),
                                                           hash_sz(hash_sz){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> projected_vec{};
            auto coeff_vec = matrix.get_coefficient_vector();

            for (const auto& [x, y]: this->point_vec)
            {
                auto sub_coeff_vec = this->get_sub_coeff_vec(coeff_vec, x);

                projected_vec.push_back(std::make_pair(x, taylor_projection::taylor_project(x,
                                                                                            sub_coeff_vec.data(), stdx::to_size_container(sub_coeff_vec.size()))));
            }

            return this->mean_square_root(projected_vec, this->point_vec);
        }

    private:

        auto get_sub_coeff_vec(const std::vector<tensor_std_float_t>& master_vec, tensor_std_float_t x) -> std::vector<tensor_std_float_t>
        {
            size_t hash_clue    = hasher::hash_reflectible(x);
            size_t hash_idx     = hash_clue % this->hash_sz;

            size_t chunk_sz     = master_vec.size() / this->hash_sz;
            size_t first        = hash_idx * chunk_sz;
            size_t last         = first + chunk_sz;

            return std::vector<tensor_std_float_t>(std::next(master_vec.begin(), first), std::next(master_vec.begin(), last));
        }

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

auto get_distributed_matrix_evaluator(const std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>>& point_vec,
                                      size_t hash_table_sz) -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
{
    return std::make_unique<HashTablePointPullMatrixEvaluator>(point_vec, hash_table_sz);
}

auto get_random_coordinated_search_optimizer_engine() -> std::unique_ptr<matrix_optimizer_subsystem::MatrixOptimizerEngineInterface>
{
    return std::make_unique<matrix_optimizer_subsystem::GenericOptimizerEngine>(matrix_optimizer_subsystem::BlockCoordinatedSearchOptimizerEngineConfigBuilder{}.set_optimization_epoch_size(size_t{1} << 8)
                                                                                                                                                                .set_optimization_step_size(size_t{1} << 5)
                                                                                                                                                                .set_optimization_loop_size(size_t{1} << 2)
                                                                                                                                                                .build());
}

void run_one_test()
{
    const size_t OPTIMIZATION_SZ                = size_t{1} << 4;

    const size_t POINT_PULL_SZ_RANGE            = size_t{1} << 3;
    const size_t TAYLOR_COEFFICIENT_SZ_RANGE    = size_t{1} << 3;

    size_t point_pull_sz    = 20u;
    size_t coefficient_sz   = 6u;
    size_t hash_table_sz    = 20u;

    std::shared_ptr<matrix_optimizer_subsystem::MatrixOptimizerEngineInterface> test_engine     = get_random_coordinated_search_optimizer_engine();

    std::shared_ptr<the_matrix::MatrixInterface> expected_matrix                                = get_taylor_matrix_table(coefficient_sz, hash_table_sz);
    std::vector<std::pair<tensor_std_float_t, tensor_std_float_t>> point_bag                    = get_random_point_bag(point_pull_sz);
    std::shared_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator                       = get_distributed_matrix_evaluator(point_bag, hash_table_sz);

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
    const size_t TEST_SZ    = size_t{1} << 12;
    const size_t COUT_SZ    = size_t{1} << 4;

    std::cout << "__BEGIN_COORDINATED_SEARCH_OPTIMIZER_ENGINE_POINT_PULL_TEST__\n";

    // matrix_optimization_big_range_test();
    // matrix_optimization_range_test();

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    std::cout << "__END_COORDINATED_SEARCH_OPTIMIZER_ENGINE_POINT_PULL_TEST__\n";
}

int main()
{
    run_test();
}