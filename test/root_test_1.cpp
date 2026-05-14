#define STRONG_MEMORY_ORDERING_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <matrix_steering_subsystem/time_machine_interface.h>
#include <matrix_steering_subsystem/time_machine_optimizer_2_factory.h>
#include <vector>
#include <random>
#include <algorithm>
#include <utility>
#include <functional>
#include <chrono>
#include <iostream>
#include <stl_extension/stdx.h>

using namespace float_def;

//10-x+\sin\left(11\cdot x\right)

class SomeEquation: public virtual time_machine::TimeMachineInterface
{
    private:

        size_t counter;

    public:

        SomeEquation(): counter(0u){}

        auto f(std_float_t x) -> tm_float_t
        {
            this->counter += 1;

            if (x > 10)
            {
                return stdx::generic_nan();
            }

            if (x > 0.1234567 && x < 0.1234568)
            {
                return x;
            }

            return std::numeric_limits<tm_float_t>::infinity();
        }

        auto get_count() -> size_t
        {
            return this->counter;
        }
};

void test_some_equation()
{
    const size_t TEST_SZ    = size_t{1} << 24;
    const size_t COUT_SZ    = size_t{1} << 12;

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

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    if (!y.has_value())
    {
        std::cout << "root not found" << std::endl;
        return;
    }

    std::cout << "<> actual_y > " << y.value() << "<> at " << x.value() << std::endl;
    std::cout << "<> TEST_SZ > " << TEST_SZ << "<> eqn_call_sz > " << some_eqn.get_count() << std::endl;
}

void test_some_equation_2()
{
    const size_t TEST_SZ    = size_t{1} << 24;
    const size_t COUT_SZ    = size_t{1} << 12;

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

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    if (!y.has_value())
    {
        std::cout << "root not found" << std::endl;
        return;
    }

    std::cout << "<> actual_y > " << y.value() << "<> at " << x.value() << std::endl;
    std::cout << "<> TEST_SZ > " << TEST_SZ << "<> eqn_call_sz > " << some_eqn.get_count() << std::endl;
}

int main()
{
    test_some_equation_2();
    test_some_equation();
}