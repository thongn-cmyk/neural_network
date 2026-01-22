#define STRONG_MEMORY_ORDERING_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "time_machine_interface.h"
#include "time_machine_optimizer_2_factory.h"
#include <vector>
#include <random>
#include <algorithm>
#include <utility>
#include <functional>
#include <chrono>
#include <iostream>
#include "stdx.h"

using namespace float_def;

//10-x+\sin\left(11\cdot x\right)

class SomeEquation: public virtual time_machine::TimeMachineInterface
{
    public:

        auto f(std_float_t x) -> tm_float_t
        {
            if (x > 10)
            {
                return stdx::generic_nan();
            }

            if (x > 0.123456 && x < 0.123457)
            {
                return x;
            }

            return std::numeric_limits<tm_float_t>::infinity();
        }
};

void test_some_equation()
{
    const size_t TEST_SZ    = size_t{1} << 20;

    std::optional<double> x = std::nullopt;
    std::optional<double> y = std::nullopt;

    SomeEquation some_eqn{};

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        auto optimizer  = global_optimality_approximator::TimeMachineOptimizerFactory::get_random_taylor_time_machine_optimizer();
        double cand_x   = optimizer->optimize(some_eqn);

        if (std::isnan(cand_x))
        {
            continue;
        }

        double cand_y   = some_eqn.f(cand_x);

        if (std::isnan(cand_y))
        {
            continue;
        }

        if (!y.has_value())
        {
            x = cand_x;
            y = cand_y;
        }

        if (std::abs(y.value()) > std::abs(cand_y))
        {
            x = cand_x;
            y = cand_y;
        }
    }

    if (!y.has_value())
    {
        std::cout << "root not found" << std::endl;
        return;
    }

    std::cout << "<> actual_y > " << y.value() << "<> at " << x.value() << std::endl;
}

void test_some_equation_2()
{
    const size_t TEST_SZ    = size_t{1} << 20;

    std::optional<double> x = std::nullopt;
    std::optional<double> y = std::nullopt;

    SomeEquation some_eqn{};

    auto factory            = global_optimality_approximator::TensorFactoryFactory::get_best_factory();

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        if (i % 10000 == 0u)
        {
            factory = global_optimality_approximator::TensorFactoryFactory::get_best_factory();
        }

        auto tensor     = factory->get();
        auto optimizer  = tensor->get();
        double cand_x   = optimizer->optimize(some_eqn);

        if (std::isnan(cand_x))
        {
            continue;
        }

        double cand_y   = some_eqn.f(cand_x);

        if (std::isnan(cand_y))
        {
            continue;
        }

        if (!y.has_value())
        {
            x = cand_x;
            y = cand_y;

            continue;
        }

        if (std::abs(y.value()) > std::abs(cand_y))
        {
            x = cand_x;
            y = cand_y;

            // tensor->feedback(1);
        }
        else
        {
            // tensor->feedback(0);
        }
    }

    if (!y.has_value())
    {
        std::cout << "root not found" << std::endl;
        return;
    }

    std::cout << "<> actual_y > " << y.value() << "<> at " << x.value() << std::endl;
}

int main()
{
    test_some_equation();
    test_some_equation_2();
}