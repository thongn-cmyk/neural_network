#define STRONG_MEMORY_ORDERING_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "time_machine_interface.h"
#include "time_machine_optimizer_factory.h"
#include <vector>
#include <random>
#include <algorithm>
#include <utility>
#include <functional>
#include <chrono>
#include <iostream>

using namespace float_def;

class RootEquation: public virtual time_machine::TimeMachineInterface
{
    private:

        std::vector<double> root_vec;

    public:

        RootEquation(std::vector<double> root_vec) noexcept: root_vec(std::move(root_vec)){}

        auto f(std_float_t x) -> tm_float_t
        {
            std::optional<tm_float_t> total = std::nullopt;

            for (double root: this->root_vec)
            {
                if (!total.has_value())
                {
                    total = x - root;
                }
                else
                {
                    total.value() *= x - root;
                }
            }

            if (!total.has_value())
            {
                throw std::runtime_error("invalid root");
            }

            return total.value();
        }
};

class OffsetRootEquation: public virtual time_machine::TimeMachineInterface
{
    private:

        std::vector<double> root_vec;
        double offset;
    
    public:

        OffsetRootEquation(std::vector<double> root_vec,
                           double offset): root_vec(std::move(root_vec)),
                                           offset(offset){}

        auto f(std_float_t x) -> tm_float_t
        {
            std::optional<tm_float_t> total = std::nullopt;

            for (double root: this->root_vec)
            {
                if (!total.has_value())
                {
                    total = x - root;
                }
                else
                {
                    total.value() *= x - root;
                }
            }

            if (!total.has_value())
            {
                throw std::runtime_error("invalid root");
            }

            return total.value() + this->offset;
        }
};

class MultiplicativeRootEquation: public virtual time_machine::TimeMachineInterface
{
    private:

        RootEquation root_eqn;
        OffsetRootEquation other_root_eqn;
    
    public:

        MultiplicativeRootEquation(RootEquation root_eqn,
                                   OffsetRootEquation other_root_eqn): root_eqn(std::move(root_eqn)),
                                                                       other_root_eqn(std::move(other_root_eqn)){}

        auto f(std_float_t x) -> tm_float_t
        {
            return root_eqn.f(x) * other_root_eqn.f(x);
        }
};

auto randomize_real() -> double
{
    const double OFFSET_VALUE_FIRST     = -10;
    const double OFFSET_VALUE_LAST      = 10;

    static auto randomizer              = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto real_distributor        = std::uniform_real_distribution<double>(OFFSET_VALUE_FIRST, OFFSET_VALUE_LAST);
    static auto real_distributor_2      = std::uniform_real_distribution<double>();
    static auto uint_distributor        = std::uniform_int_distribution<size_t>();

    if (uint_distributor(randomizer) % 2 == 0u)
    {
        return real_distributor(randomizer);
    }
    else
    {
        return real_distributor_2(randomizer);
    }
}

auto randomize_root_vec() -> std::vector<double>
{
    const size_t ROOT_SZ_RANGE      = 10u;
    static auto randomizer          = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor    = std::uniform_int_distribution<size_t>();

    size_t root_sz                  = uint_distributor(randomizer) % ROOT_SZ_RANGE + 1u;
    auto result                     = std::vector<double>();

    for (size_t i = 0u; i < root_sz; ++i)
    {
        result.push_back(randomize_real());
    }

    return result;
}

auto best_root(double x, const std::vector<double>& root_vec) -> double
{
    if (std::isnan(x))
    {
        throw std::invalid_argument("bad x, Not a Number");
    }

    for (double root: root_vec)
    {
        if (std::isnan(root))
        {
            throw std::invalid_argument("bad root, Not a Number");
        }
    }

    std::vector<std::pair<double, double>> dist_vec{};

    for (double root: root_vec)
    {
        dist_vec.push_back({std::abs(root - x), root});
    }

    std::sort(dist_vec.begin(), dist_vec.end());

    if (dist_vec.empty())
    {
        throw std::invalid_argument("bad root, empty vector");
    }

    return dist_vec[0].second;
}

auto run_one_root_test() -> double
{
    const size_t TEST_SZ                        = size_t{1} << 6;
    std::vector<double> root_vec                = randomize_root_vec();

    std::optional<double> optimized_root_x      = std::nullopt;
    std::optional<double> optimized_root_y      = std::nullopt;

    RootEquation root_eqn(root_vec);

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> optimizer = global_optimality_approximator::TimeMachineOptimizerFactory::get_random_taylor_time_machine_optimizer();

        double local_root_x = optimizer->optimize(root_eqn);
        double local_root_y = root_eqn.f(local_root_x);

        if (!std::isnan(local_root_x) && !std::isnan(local_root_y))
        {
            if (!optimized_root_x.has_value())
            {
                optimized_root_x    = local_root_x;
                optimized_root_y    = local_root_y;
            }

            if (std::abs(optimized_root_y.value()) > std::abs(local_root_y))
            {
                optimized_root_x    = local_root_x;
                optimized_root_y    = local_root_y;
            }
        }
    }

    if (!optimized_root_x.has_value())
    {
        std::cout << "optimized root > null " << std::endl;;
        return 0;
    }

    double actual_root  = best_root(optimized_root_x.value(), root_vec);
    double perc         = optimized_root_x.value() / actual_root;

    std::cout << "optimized_root > " << optimized_root_x.value() << "<> actual_root > " << actual_root << "<>" << "perc > " << perc << std::endl;

    return perc;
}

void run_root_test()
{
    const size_t TEST_SZ    = size_t{1} << 6;
    double total            = 0;

    std::cout << "__BEGIN_ROOT_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        total += run_one_root_test();
    }

    double normalized_total = total / TEST_SZ;
    std::cout << "root percentage > " << normalized_total << std::endl;

    std::cout << "__END_ROOT_TEST__" << std::endl;
}

auto run_one_root_offset_test() -> double
{
    const size_t TEST_SZ                        = size_t{1} << 6;
    std::vector<double> root_vec                = randomize_root_vec();
    std::vector<double> other_root_vec          = randomize_root_vec();
    double offset                               = randomize_real();

    std::optional<double> optimized_root_x      = std::nullopt;
    std::optional<double> optimized_root_y      = std::nullopt;

    RootEquation org_root_eqn(root_vec);
    OffsetRootEquation offset_root_eqn(other_root_vec, offset);
    MultiplicativeRootEquation root_eqn(org_root_eqn, offset_root_eqn);

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> optimizer = global_optimality_approximator::TimeMachineOptimizerFactory::get_random_taylor_time_machine_optimizer();

        double local_root_x = optimizer->optimize(root_eqn);
        double local_root_y = root_eqn.f(local_root_x);

        if (!std::isnan(local_root_x) && !std::isnan(local_root_y))
        {
            if (!optimized_root_x.has_value())
            {
                optimized_root_x    = local_root_x;
                optimized_root_y    = local_root_y;
            }

            if (std::abs(optimized_root_y.value()) > std::abs(local_root_y))
            {
                optimized_root_x    = local_root_x;
                optimized_root_y    = local_root_y;
            }
        }
    }

    if (!optimized_root_x.has_value())
    {
        std::cout << "optimized root > null " << std::endl;;
        return 0;
    }

    double root_deviation = root_eqn.f(optimized_root_x.value());

    std::cout << "optimized_root > " << optimized_root_x.value() << "<> root_deviation > " << root_deviation << "<>" << std::endl;

    return root_deviation;
}

void run_root_offset_test()
{
    const size_t TEST_SZ    = size_t{1} << 6;
    double total            = 0;

    std::cout << "__BEGIN_ROOT_OFFSET_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        total += run_one_root_offset_test();
    }

    double normalized_total = total / TEST_SZ;
    std::cout << "root deviation > " << normalized_total << std::endl;

    std::cout << "__END_ROOT_OFFSET_TEST__" << std::endl;
}

int main()
{
    run_root_offset_test();
    run_root_test();
}