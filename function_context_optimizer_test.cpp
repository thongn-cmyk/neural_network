#define STRONG_MEMORY_ORDERING_FLAG true

#include "function_context_optimizer.h"
#include "score_context_optimizer.h"
#include "global_optimality_approximator.h"
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

using namespace float_def;

//do you understand what software engineering is about?
//it's about compromision of the working context, and integration of the compoennts
//the reason that it is written like this is mainly for extensions and maintainability, we dont know if the context will be preferably changed in some scenerios or not

class MagicMachine: public virtual global_optimality_approximator::TensorFactoryInterface,
                    public virtual score_context_optimizer::StatisticalMachineInterface
{
    private:

        std::unique_ptr<global_optimality_approximator::TensorFactoryInterface> base;

    public:

        MagicMachine(std::unique_ptr<global_optimality_approximator::TensorFactoryInterface> base): base(std::move(base)){}

        auto get() -> std::unique_ptr<global_optimality_approximator::FactoryTensorInterface>
        {
            return this->base->get();
        }
};

class MagicMachineFactory: public virtual score_context_optimizer::StatisticalMachineGeneratorInterface
{
    public:

        auto get() -> std::unique_ptr<score_context_optimizer::StatisticalMachineInterface>
        {
            return std::make_unique<MagicMachine>(global_optimality_approximator::TensorFactoryFactory::get_best_factory());            
        }
};

class MagicMachine2: public virtual function_context_optimizer::StatisticalMachineInterface
{
    private:

        std::unique_ptr<score_context_optimizer::IterativeContextGeneratorInterface> base;

    public:

        MagicMachine2(std::unique_ptr<score_context_optimizer::IterativeContextGeneratorInterface> base): base(std::move(base)){}

        auto get() -> const std::unique_ptr<score_context_optimizer::IterativeContextGeneratorInterface>&
        {
            return this->base;
        }
};

class MagicMachine2Factory: public virtual function_context_optimizer::StatisticalMachineGeneratorInterface
{
    public:

        auto get() -> std::unique_ptr<function_context_optimizer::StatisticalMachineInterface>
        {
            return std::make_unique<MagicMachine2>(score_context_optimizer::ContextOptimizerFactory::get_best_binary_progress_context_optimizer(std::make_unique<MagicMachineFactory>()));
        }
};

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

class FunctionWrapper: public virtual function_context_optimizer::FunctionInterface
{
    private:

        time_machine::TimeMachineInterface& base;
    
    public:

        FunctionWrapper(time_machine::TimeMachineInterface& base): base(base){}

        auto f(function_context_optimizer::ctx_float_t x) -> function_context_optimizer::ctx_float_t
        {
            return this->base.f(x);
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

    const double OFFSET_VALUE_FIRST_1   = -0.000001;
    const double OFFSET_VALUE_LAST_1    = 0.000001;

    static auto randomizer              = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto real_distributor        = std::uniform_real_distribution<double>(OFFSET_VALUE_FIRST, OFFSET_VALUE_LAST);
    static auto real_distributor_2      = std::uniform_real_distribution<double>();
    static auto real_distributor_3      = std::uniform_real_distribution<double>(OFFSET_VALUE_FIRST_1, OFFSET_VALUE_LAST_1);
    static auto uint_distributor        = std::uniform_int_distribution<size_t>();

    size_t dispatch_code                = uint_distributor(randomizer) % 5;

    switch (dispatch_code)
    {
        case 0:
        {
            return real_distributor(randomizer);
        }
        case 1:
        {
            return real_distributor_2(randomizer);
        }
        case 2:
        {
            return real_distributor_3(randomizer);
        }
        case 3:
        {
            return real_distributor_3(randomizer);
        }
        case 4:
        {
            return real_distributor_3(randomizer);
        }
        default:
        {
            std::unreachable();
        }
    }
}

auto randomize_root_vec() -> std::vector<double>
{
    const size_t ROOT_SZ_RANGE      = 100u;
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

auto randomize_root_vec_2() -> std::vector<double>
{
    const size_t ROOT_SZ_RANGE      = 2u;
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
    const size_t TEST_SZ                        = size_t{1} << 8;
    std::vector<double> root_vec                = randomize_root_vec();

    std::optional<double> optimized_root_x      = std::nullopt;
    std::optional<double> optimized_root_y      = std::nullopt;

    static std::unique_ptr<function_context_optimizer::FunctionContextOptimizerInterface> tensor_factory_2 = function_context_optimizer::ContextOptimizerFactory::get_best_context_optimizer(std::make_unique<MagicMachine2Factory>());

    RootEquation root_eqn(root_vec);
    FunctionWrapper func(root_eqn);

    const auto& tensor_factory  = std::dynamic_pointer_cast<MagicMachine2>(tensor_factory_2->optimize_context(func))->get();
    auto iteration_ctx          = tensor_factory->get();

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        auto tensor         = iteration_ctx->next();
        auto factory        = std::dynamic_pointer_cast<global_optimality_approximator::TensorFactoryInterface>(tensor->get_statistical_machine());
        auto tensor_2       = factory->get();

        std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> optimizer = tensor_2->get();

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

                tensor->feedback(1);
                tensor_2->feedback(1);
            }
            else
            {
                tensor->feedback(0);
                tensor_2->feedback(0);              
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
    const size_t TEST_SZ    = size_t{1} << 10;
    double total            = 0;

    std::cout << "__BEGIN_ROOT_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ * 4; ++i)
    {
        run_one_root_test();
    }

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
    const size_t TEST_SZ                        = size_t{1} << 8;
    std::vector<double> root_vec                = randomize_root_vec();
    std::vector<double> other_root_vec          = randomize_root_vec_2();
    double offset                               = randomize_real();

    std::optional<double> optimized_root_x      = std::nullopt;
    std::optional<double> optimized_root_y      = std::nullopt;

    RootEquation org_root_eqn(root_vec);
    OffsetRootEquation offset_root_eqn(other_root_vec, offset);
    MultiplicativeRootEquation root_eqn(org_root_eqn, offset_root_eqn);
    FunctionWrapper func(root_eqn);

    static std::unique_ptr<function_context_optimizer::FunctionContextOptimizerInterface> tensor_factory_2 = function_context_optimizer::ContextOptimizerFactory::get_best_context_optimizer(std::make_unique<MagicMachine2Factory>());

    const auto& tensor_factory  = std::dynamic_pointer_cast<MagicMachine2>(tensor_factory_2->optimize_context(func))->get();
    auto iteration_ctx          = tensor_factory->get();


    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        auto tensor         = iteration_ctx->next();
        auto factory        = std::dynamic_pointer_cast<global_optimality_approximator::TensorFactoryInterface>(tensor->get_statistical_machine());
        auto tensor_2       = factory->get();

        std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> optimizer = tensor_2->get();

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

                tensor->feedback(1);
                tensor_2->feedback(1);
            }
            else
            {
                tensor->feedback(0);
                tensor_2->feedback(0);
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
    const size_t TEST_SZ    = size_t{1} << 10;
    double total            = 0;

    std::cout << "__BEGIN_ROOT_OFFSET_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_root_offset_test();
    }

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