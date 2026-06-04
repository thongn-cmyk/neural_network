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
#include <stl_extension/stdx.h>
#include <taylor_matrix/host_matrix/the_host_matrix.h>
#include <stl_extension/stdx.h>
#include <limits.h>
#include <bit>

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

struct MultidimensionalFrame
{
    std::vector<tensor_std_float_t> frame;
};

auto to_bit_vector(tensor_std_float_t x) -> std::vector<bool>
{
    constexpr size_t bit_vector_sz                          = std::numeric_limits<tensor_std_float_t>::digits;
    std::array<char, sizeof(tensor_std_float_t)> byte_arr   = std::bit_cast<std::array<char, sizeof(tensor_std_float_t)>>(x);
    std::vector<bool> rs                                    = {};

    for (size_t i = 0u; i < bit_vector_sz; ++i)
    {
        size_t slot_idx = i / bit_vector_sz;
        size_t slot_off = i % bit_vector_sz;
        bool bit_togg   = std::bit_cast<uint8_t>(byte_arr[slot_idx]) & (uint8_t{1} << slot_off);

        rs.push_back(bit_togg);
    }

    return rs;
}

auto to_tensor_vector(const std::vector<bool>& bool_vec) -> std::vector<tensor_std_float_t>
{
    std::vector<tensor_std_float_t> rs{};

    for (bool e: bool_vec)
    {
        rs.push_back(e);
    }

    return rs;
}

auto to_multidimensional_value(tensor_std_float_t e) -> MultidimensionalFrame
{
    return MultidimensionalFrame
    {
        to_tensor_vector(to_bit_vector(e))
    };
}

class MatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::vector<std::pair<MultidimensionalFrame, MultidimensionalFrame>> point_vec;
        std::vector<size_t> matrix_shape;

    public:

        MatrixEvaluator(std::vector<std::pair<MultidimensionalFrame, MultidimensionalFrame>> point_vec): point_vec(std::move(point_vec)){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {

        }
};

auto get_matrix_evaluator(const std::vector<std::pair<MultidimensionalFrame, MultidimensionalFrame>>& point_vec,
                          const std::vector<size_t>& matrix_shape) -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
{
    return std::make_unique<MatrixEvaluator>(point_vec, matrix_shape);
}

auto get_random_coordinated_search_optimizer_engine() -> std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
{
    return std::make_unique<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
    (
        matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngineConfig
        {
            .matrix_cache_map_cap                       = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .time_machine_cache_map_cap                 = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .optimization_epoch_sz                      = 16ULL,
            .optimization_step_sz                       = 16ULL,
            .optimization_loop_sz                       = 4ULL
        }
    );
}

//today we'd add hash_dispatch + single value projections to increase stability of projections
//let's try

void run_one_test()
{

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