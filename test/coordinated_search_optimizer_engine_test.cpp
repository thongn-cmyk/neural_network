#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <matrix_optimizer_subsystem/coordinated_search_optimizer_engine.h>
#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <random>
#include <functional>
#include <algorithm>
#include <iostream>
#include <memory>
#include <matrix/tensor_model.h>
#include <general_definition/float_def.h>
#include <math.h>
#include <type_traits>

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

auto randomize_byte_width() -> size_t
{
    size_t choice = randomize_int(0, 3);

    if (choice == 0)
    {
        return 4u;
    }
    else if (choice == 1)
    {
        return 8u;
    }
    else if (choice == 2)
    {
        return 16u;
    }
    else
    {
        std::cout << "mayday, choice enumeration out of range\n";
        std::abort();
    }
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

auto get_random_coordinated_search_optimizer_engine() -> std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
{
    return std::make_unique<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
    (
        matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngineConfig
        {
            .matrix_cache_map_cap                       = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .time_machine_cache_map_cap                 = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .optimization_epoch_sz                      = static_cast<uint64_t>(randomize_int(0, size_t{1} << 4)),
            .optimization_step_sz                       = static_cast<uint64_t>(randomize_int(0, size_t{1} << 4)),
            .optimization_loop_sz                       = randomize_optional_int<uint64_t>(0, size_t{1} << 4)
            // .coefficient_projector_float_byte_width     = (randomize_int(0, 1) == 0) ? std::optional<uint64_t>(std::nullopt)
            //                                                                          : std::optional<uint64_t>(randomize_byte_width()),

            // .time_machine_optimizer_float_byte_width    = (randomize_int(0, 1) == 0) ? std::optional<uint64_t>(std::nullopt),
            //                                                                          : std::optional<uint64_t>(randomize_byte_width())
        }
    );
}

class FlatMatrix: public virtual the_matrix::MatrixInterface
{
    private:

        std::vector<tensor_std_float_t> tensor_vec;
    
    public:

        FlatMatrix(std::vector<tensor_std_float_t> tensor_vec): tensor_vec(std::move(tensor_vec)){}

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
            return std::make_shared<FlatMatrix>(*this);
        }
};

auto randomize_tensor_vec_for_size_of(size_t sz) -> std::vector<tensor_std_float_t>
{
    std::vector<tensor_std_float_t> rs{};

    const double VALUE_FIRST    = -10;
    const double VALUE_LAST     = 10;

    for (size_t i = 0u; i < sz; ++i)
    {
        rs.push_back(randomize_double(VALUE_FIRST, VALUE_LAST));
    }

    return rs;
}

auto get_random_matrix() -> std::unique_ptr<the_matrix::MatrixInterface>
{
    const size_t TENSOR_SZ_RANGE    = size_t{1} << 2;
    size_t tensor_sz                = (randomize_int(0, TENSOR_SZ_RANGE) + 1) * 64;

    // std::vector<tensor_std_float_t> tensor_vec{};

    // for (size_t i = 0u; i < tensor_sz; ++i)
    // {
    //     tensor_vec.push_back(randomize_double(-10, 10));
    // }

    return std::make_unique<FlatMatrix>(randomize_tensor_vec_for_size_of(tensor_sz));
}

auto get_expected_matrix_like(const std::shared_ptr<the_matrix::MatrixInterface>& matrix) -> std::unique_ptr<the_matrix::MatrixInterface>
{
    return std::make_unique<FlatMatrix>(randomize_tensor_vec_for_size_of(matrix->get_coefficient_vector().size()));
}

class SquareDeviationMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::shared_ptr<the_matrix::MatrixInterface> expected;
    
    public:

        SquareDeviationMatrixEvaluator(std::shared_ptr<the_matrix::MatrixInterface> expected): expected(std::move(expected)){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<tensor_std_float_t> expected_tensor_vec = this->expected->get_coefficient_vector();
            std::vector<tensor_std_float_t> compared_tensor_vec = matrix.get_coefficient_vector();

            return this->mean_square_root(expected_tensor_vec, compared_tensor_vec);
        }

    private:
        
        auto mean_square_root(const std::vector<tensor_std_float_t>& expected_tensor_vec,
                              const std::vector<tensor_std_float_t>& compared_tensor_vec) -> eval_float_t
        {
            if (expected_tensor_vec.size() != compared_tensor_vec.size())
            {
                std::cout << "mayday, mismatched tensor dimension size\n";
                std::abort();
            }

            eval_float_t rs = 0;

            for (size_t i = 0u; i < expected_tensor_vec.size(); ++i)
            {
                rs += std::pow(expected_tensor_vec[i] - compared_tensor_vec[i], 2);
            }

            if (expected_tensor_vec.size() != 0u)
            {
                rs /= expected_tensor_vec.size();
            }

            return std::sqrt(rs);
        }
};

auto get_matrix_difference_evaluator(const std::shared_ptr<the_matrix::MatrixInterface>& expected) -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
{
    return std::make_unique<SquareDeviationMatrixEvaluator>(expected);
}

void run_one_test()
{
    std::shared_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> test_engine   = get_random_coordinated_search_optimizer_engine();

    std::shared_ptr<the_matrix::MatrixInterface> expected_matrix                                = get_random_matrix();
    std::shared_ptr<the_matrix::MatrixInterface> random_matrix                                  = get_expected_matrix_like(expected_matrix);

    std::shared_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator                       = get_matrix_difference_evaluator(expected_matrix);

    common_exception::CancellationToken cancellation_token{};

    std::shared_ptr<the_matrix::MatrixInterface> output_matrix                                  = test_engine->optimize(*random_matrix, *evaluator, cancellation_token);

    double initial_deviation    = evaluator->get_deviation(*random_matrix);
    double optimized_deviation  = evaluator->get_deviation(*output_matrix);

    std::cout << "initial_deviation > " << initial_deviation << "<> optimized_deviation > " << optimized_deviation << "\n";
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 12;
    const size_t COUT_SZ    = size_t{1} << 4;

    std::cout << "__BEGIN_COORDINATED_SEARCH_OPTIMIZER_ENGINE_TEST__\n";

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    std::cout << "__END_COORDINATED_SEARCH_OPTIMIZER_ENGINE_TEST__\n";
}

int main()
{
    run_test();
}