#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <matrix_steering_subsystem/range_optimizer.h>
#include <chrono>
#include <random>
#include <iostream>
#include <functional>
#include <algorithm>
#include <limits.h>

auto randomize_int(size_t first, size_t last)
{
    if (first >= last)
    {
        std::abort();
    }

    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return first + (randomizer() % (last - first));
}

auto randomize_double() -> double
{
    const double REAL_FIRST = -10;
    const double REAL_LAST  = 10;

    static auto distributor = std::uniform_real_distribution<double>(REAL_FIRST, REAL_LAST);
    static auto randomizer  = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};

    return distributor(randomizer);
}

auto randomize_prediction_range() -> size_t
{
    return randomize_int(0u, size_t{1} << 8);
}

void run_one_test()
{
    const size_t TEST_SZ    = size_t{1} << 10;

    size_t test_range = randomize_prediction_range();
    std::unique_ptr<range_optimizer::RangePredictorInterface> range_optimizer = std::make_unique<range_optimizer::ExponentialRangePredictor>(test_range);

    if (range_optimizer->size() != test_range)
    {
        std::cout << "mayday, mismatched initialization range\n";
        std::abort();
    }
    
    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        std::unique_ptr<range_optimizer::RangePredictionResultInterface> range_prediction_result = range_optimizer->next();
        size_t predicted_range = range_prediction_result->get_range();

        if (randomize_int(0u, 1) == 0u)
        {
            range_prediction_result->feedback(randomize_double());
        }
        else
        {
            if (randomize_int(0u, 1) == 0u)
            {
                range_prediction_result->feedback(std::numeric_limits<double>::quiet_NaN());
            }
            else
            {
                range_prediction_result->feedback(std::numeric_limits<double>::infinity());
            }
        }

        if (predicted_range > range_optimizer->size())
        {
            std::cout << "mayday, bad range prediction result\n";
            std::abort();
        }
    }
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }
}

int main()
{
    run_test();
}
