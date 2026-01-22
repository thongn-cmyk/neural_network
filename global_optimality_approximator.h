//HEADER_CONTROL 3

#ifndef __GLOBAL_OPTIMALITY_APPROXIMATOR_H__
#define __GLOBAL_OPTIMALITY_APPROXIMATOR_H__

#include "stdx.h"
#include "float_def.h"
#include "global_optimality_approximator_interface.h"
#include "local_optimality_approximator_interface.h"
#include "conventional_randomizer.h"
#include <optional>

namespace global_optimality_approximator
{
    using std_float_t   = float_def::std_float_t;
    using tm_float_t    = float_def::tm_float_t;

    class LinearTimeMachineOptimizer: public virtual TimeMachineOptimizerInterface
    {
        private:

            std_float_t seed;
            std_float_t step;
            size_t step_count;
            std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface> optimality_approximator;

        public:

            LinearTimeMachineOptimizer(std_float_t seed,
                                       std_float_t step,
                                       size_t step_count,
                                       std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface> optimality_approximator) noexcept: seed(seed),
                                                                                                                                                          step(step),
                                                                                                                                                          step_count(step_count),
                                                                                                                                                          optimality_approximator(std::move(optimality_approximator)){}

            auto optimize(time_machine::TimeMachineInterface& time_machine) -> std_float_t
            {
                std::optional<std_float_t> best_x   = std::nullopt;
                std::optional<tm_float_t> best_y    = std::nullopt;

                for (size_t i = 0u; i < this->step_count; ++i)
                {
                    std_float_t x = this->seed + i * this->step;

                    if (std::isnan(x))
                    {
                        continue;
                    }

                    std_float_t new_x = this->optimality_approximator->approx_x(time_machine, x);

                    if (std::isnan(new_x))
                    {
                        continue;
                    }

                    tm_float_t new_y = time_machine.f(new_x);

                    if (std::isnan(new_y))
                    {
                        continue;
                    }

                    if (!best_y.has_value())
                    {
                        best_y = new_y;
                        best_x = new_x;
                    }

                    if (std::abs(best_y.value()) > std::abs(new_y))
                    {
                        best_y = new_y;
                        best_x = new_x;
                    }
                }

                if (!best_x.has_value())
                {
                    return stdx::generic_nan();
                }

                return best_x.value();
            }
    };

    class ExponentialTimeMachineOptimizer: public virtual TimeMachineOptimizerInterface
    {
        private:

            std_float_t seed;
            std_float_t exp_base;
            size_t step_count;
            std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface> optimality_approximator;

        public:

            ExponentialTimeMachineOptimizer(std_float_t seed,
                                            std_float_t exp_base,
                                            size_t step_count,
                                            std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface> optimality_approximator) noexcept: seed(seed),
                                                                                                                                                               exp_base(exp_base),
                                                                                                                                                               step_count(step_count),
                                                                                                                                                               optimality_approximator(std::move(optimality_approximator)){}

            auto optimize(time_machine::TimeMachineInterface& time_machine) -> std_float_t
            {
                std::optional<std_float_t> best_x   = std::nullopt;
                std::optional<tm_float_t> best_y    = std::nullopt;

                for (size_t i = 0u; i < this->step_count; ++i)
                {
                    std_float_t x = this->seed + std::pow(this->exp_base, i);

                    if (std::isnan(x))
                    {
                        continue;
                    }

                    std_float_t new_x = this->optimality_approximator->approx_x(time_machine, x);

                    if (std::isnan(new_x))
                    {
                        continue;
                    }

                    tm_float_t new_y = time_machine.f(new_x);

                    if (std::isnan(new_y))
                    {
                        continue;
                    }

                    if (!best_y.has_value())
                    {
                        best_y = new_y;
                        best_x = new_x;
                    }

                    if (std::abs(best_y.value()) > std::abs(new_y))
                    {
                        best_y = new_y;
                        best_x = new_x;
                    }
                }

                if (!best_x.has_value())
                {
                    return stdx::generic_nan();
                }

                return best_x.value();
            }
    };

    template <class DeltaFunction>
    class ForwardIterativeTimeMachineOptimizer: public virtual TimeMachineOptimizerInterface
    {
        private:

            std_float_t seed;
            DeltaFunction func;
            std_float_t iterative_epsilon;
            size_t step_count;
            size_t iteration_count;
            std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface> optimality_approximator;
        
        public:

            ForwardIterativeTimeMachineOptimizer(std_float_t seed,
                                                 DeltaFunction func,
                                                 std_float_t iterative_epsilon,
                                                 size_t step_count,
                                                 size_t iteration_count,
                                                 std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface> optimality_approximator) noexcept: seed(seed),
                                                                                                                                                                    func(func),
                                                                                                                                                                    iterative_epsilon(iterative_epsilon),
                                                                                                                                                                    step_count(step_count),
                                                                                                                                                                    iteration_count(iteration_count),
                                                                                                                                                                    optimality_approximator(std::move(optimality_approximator)){}

            auto optimize(time_machine::TimeMachineInterface& time_machine) -> std_float_t
            {
                std::optional<std_float_t> best_x   = std::nullopt;
                std::optional<tm_float_t> best_y    = std::nullopt;
                std_float_t cur_seed                = seed;

                for (size_t i = 0u; i < this->iteration_count; ++i)
                {
                    std::optional<std_float_t> next_seed = std::nullopt;

                    for (size_t j = 0u; j < this->step_count; ++j)
                    {
                        std_float_t x = cur_seed + this->func(j);

                        if (std::isnan(x))
                        {
                            continue;
                        }

                        std_float_t new_x = this->optimality_approximator->approx_x(time_machine, x);

                        if (std::isnan(new_x))
                        {
                            continue;
                        }

                        tm_float_t new_y = time_machine.f(new_x);

                        if (std::isnan(new_y))
                        {
                            continue;
                        }

                        if (!best_y.has_value())
                        {
                            best_y  = new_y;
                            best_x  = new_x;
                        }

                        if (std::abs(best_y.value()) > std::abs(new_y))
                        {
                            best_y  = new_y;
                            best_x  = new_x;
                        }

                        if (new_x > cur_seed + this->iterative_epsilon)
                        {
                            if (!next_seed.has_value())
                            {
                                next_seed = new_x;
                            }

                            if (next_seed.value() > new_x)
                            {
                                next_seed = new_x;
                            }
                        }
                    }

                    if (!next_seed.has_value())
                    {
                        break;
                    }

                    cur_seed = next_seed.value();
                }

                if (!best_x.has_value())
                {
                    return stdx::generic_nan();
                }

                return best_x.value();
            }
    };

    template <class DeltaFunction>
    class BackwardIterativeTimeMachineOptimizer: public virtual TimeMachineOptimizerInterface
    {
        private:

            std_float_t seed;
            DeltaFunction func;
            std_float_t iterative_epsilon;
            size_t step_count;
            size_t iteration_count;
            std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface> optimality_approximator;

        public:

            BackwardIterativeTimeMachineOptimizer(std_float_t seed,
                                                  DeltaFunction func,
                                                  std_float_t iterative_epsilon,
                                                  size_t step_count,
                                                  size_t iteration_count,
                                                  std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface> optimality_approximator) noexcept: seed(seed),
                                                                                                                                                                     func(std::move(func)),
                                                                                                                                                                     iterative_epsilon(iterative_epsilon),
                                                                                                                                                                     step_count(step_count),
                                                                                                                                                                     iteration_count(iteration_count),
                                                                                                                                                                     optimality_approximator(std::move(optimality_approximator)){}

            auto optimize(time_machine::TimeMachineInterface& time_machine) -> std_float_t
            {
                std::optional<std_float_t> best_x   = std::nullopt;
                std::optional<tm_float_t> best_y    = std::nullopt;
                std_float_t cur_seed                = seed;

                for (size_t i = 0u; i < this->iteration_count; ++i)
                {
                    std::optional<std_float_t> next_seed = std::nullopt;

                    for (size_t j = 0u; j < this->step_count; ++j)
                    {
                        std_float_t x = cur_seed - this->func(j);

                        if (std::isnan(x))
                        {
                            continue;
                        }

                        std_float_t new_x = this->optimality_approximator->approx_x(time_machine, x);

                        if (std::isnan(new_x))
                        {
                            continue;
                        }

                        tm_float_t new_y = time_machine.f(new_x);

                        if (std::isnan(new_y))
                        {
                            continue;
                        }

                        if (!best_y.has_value())
                        {
                            best_y  = new_y;
                            best_x  = new_x;
                        }

                        if (std::abs(best_y.value()) > std::abs(new_y))
                        {
                            best_y  = new_y;
                            best_x  = new_x;
                        }

                        if (new_x < cur_seed - this->iterative_epsilon)
                        {
                            if (!next_seed.has_value())
                            {
                                next_seed = new_x;
                            }

                            if (next_seed.value() < new_x)
                            {
                                next_seed = new_x;
                            }
                        }
                    }

                    if (!next_seed.has_value())
                    {
                        break;
                    }

                    cur_seed = next_seed.value();
                }

                if (!best_x.has_value())
                {
                    return stdx::generic_nan();
                }

                return best_x.value();
            }
    };

    class TimeMachineOptimizerSetOptimizer: public virtual TimeMachineOptimizerInterface
    {
        private:

            std::vector<std::unique_ptr<TimeMachineOptimizerInterface>> time_machine_optimizer_set;

        public:

            TimeMachineOptimizerSetOptimizer(std::vector<std::unique_ptr<TimeMachineOptimizerInterface>> time_machine_optimizer_set) noexcept: time_machine_optimizer_set(std::move(time_machine_optimizer_set)){}

            auto optimize(time_machine::TimeMachineInterface& time_machine) -> std_float_t
            {
                std::optional<std_float_t> best_x   = std::nullopt;
                std::optional<std_float_t> best_y   = std::nullopt;

                for (const auto& time_machine_optimizer: this->time_machine_optimizer_set)
                {
                    std_float_t local_x = time_machine_optimizer->optimize(time_machine);

                    if (std::isnan(local_x))
                    {
                        continue;
                    }

                    tm_float_t local_y  = time_machine.f(local_x);

                    if (std::isnan(local_y))
                    {
                        continue;
                    }

                    if (!best_y.has_value())
                    {
                        best_x = local_x;
                        best_y = local_y;
                    }

                    if (std::abs(local_y) < std::abs(best_y.value()))
                    {
                        best_x = local_x;
                        best_y = local_y;
                    }
                }

                if (!best_x.has_value())
                {
                    return stdx::generic_nan();
                }

                return best_x.value();
            }
    };

    class LinearDeltaStepFunction
    {
        private:

            std_float_t a;
        
        public:

            LinearDeltaStepFunction(std_float_t a): a(a){}

            constexpr auto operator()(size_t i) const noexcept -> std_float_t
            {
                return this->a * i;
            }

    };

    class ChaoticDeltaStepFunction
    {
        private:

            conventional_randomizer::ApplicationRandomizerObject randomizer;
            std_float_t a;

        public:

            ChaoticDeltaStepFunction(std_float_t a): randomizer(),
                                                     a(a){}

            constexpr auto operator()(size_t i) -> std_float_t
            {
                return this->randomizer.ld_randomize_percentage_focal() * this->a;
            }
    };

    class ExponentialDeltaStepFunction
    {
        private:

            std_float_t exp_base;

        public:

            ExponentialDeltaStepFunction(std_float_t exp_base): exp_base(exp_base){}

            constexpr auto operator()(size_t i) const noexcept -> std_float_t
            {
                return std::pow(this->exp_base, i);
            }
    };

    class RawMachineFactory
    {
        private:

            template <class DeltaFunction>
            static auto get_forward_it_time_machine_optimizer(std_float_t seed,
                                                              DeltaFunction&& func,
                                                              std_float_t iterative_epsilon,
                                                              size_t step_count,
                                                              size_t iteration_count,
                                                              std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& optimality_approximator) -> std::unique_ptr<TimeMachineOptimizerInterface>
            {
                using decay_function_t = std::decay_t<DeltaFunction>;

                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (std::isnan(iterative_epsilon))
                {
                    throw std::invalid_argument("bad iterative epsilon, NaN");
                }

                if (optimality_approximator == nullptr)
                {
                    throw std::invalid_argument("bad optimality approximator, null");
                }

                return std::make_unique<ForwardIterativeTimeMachineOptimizer<decay_function_t>>(seed,
                                                                                                std::forward<DeltaFunction>(func),
                                                                                                iterative_epsilon,
                                                                                                step_count,
                                                                                                iteration_count,
                                                                                                std::move(optimality_approximator));
            }

            template <class DeltaFunction>
            static auto get_backward_it_time_machine_optimizer(std_float_t seed,
                                                               DeltaFunction&& func,
                                                               std_float_t iterative_epsilon,
                                                               size_t step_count,
                                                               size_t iteration_count,
                                                               std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& optimality_approximator) -> std::unique_ptr<TimeMachineOptimizerInterface>
            {
                using decay_function_t = std::decay_t<DeltaFunction>;

                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (std::isnan(iterative_epsilon))
                {
                    throw std::invalid_argument("bad iterative epsilon, NaN");
                }

                if (optimality_approximator == nullptr)
                {
                    throw std::invalid_argument("bad optimality approximator, null");
                }

                return std::make_unique<BackwardIterativeTimeMachineOptimizer<decay_function_t>>(seed,
                                                                                                 std::forward<DeltaFunction>(func),
                                                                                                 iterative_epsilon,
                                                                                                 step_count,
                                                                                                 iteration_count,
                                                                                                 std::move(optimality_approximator));
            }

            template <class DeltaFunction>
            static auto get_it_time_machine_optimizer(std_float_t seed,
                                                      DeltaFunction&& func,
                                                      std_float_t iterative_epsilon,
                                                      size_t step_count,
                                                      size_t iteration_count,
                                                      bool direction,
                                                      std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& optimality_approximator) -> std::unique_ptr<TimeMachineOptimizerInterface>
            {
                if (direction)
                {
                    return get_forward_it_time_machine_optimizer(seed,
                                                                 std::forward<DeltaFunction>(func),
                                                                 iterative_epsilon,
                                                                 step_count,
                                                                 iteration_count,
                                                                 std::move(optimality_approximator));
                }
                else
                {
                    return get_backward_it_time_machine_optimizer(seed,
                                                                  std::forward<DeltaFunction>(func),
                                                                  iterative_epsilon,
                                                                  step_count,
                                                                  iteration_count,
                                                                  std::move(optimality_approximator));
                }
            }

        public:

            static auto get_linear_time_machine_optimizer(std_float_t seed,
                                                          std_float_t step,
                                                          size_t step_count,
                                                          std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& optimality_approximator) -> std::unique_ptr<TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (std::isnan(step))
                {
                    throw std::invalid_argument("bad step, NaN");
                }

                if (optimality_approximator == nullptr)
                {
                    throw std::invalid_argument("bad approximator, null");
                }

                return std::make_unique<LinearTimeMachineOptimizer>(seed,
                                                                    step,
                                                                    step_count,
                                                                    std::move(optimality_approximator));
            }

            static auto get_exponential_time_machine_optimizer(std_float_t seed,
                                                               std_float_t exp_base,
                                                               size_t step_count,
                                                               std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& optimality_approximator) -> std::unique_ptr<TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (std::isnan(exp_base))
                {
                    throw std::invalid_argument("bad exp_base, NaN");
                }

                if (optimality_approximator == nullptr)
                {
                    throw std::invalid_argument("bad approximator, null");
                }

                return std::make_unique<ExponentialTimeMachineOptimizer>(seed,
                                                                         exp_base,
                                                                         step_count,
                                                                         std::move(optimality_approximator));
            }

            static auto get_exponential_iterative_time_machine_optimizer(std_float_t seed,
                                                                         std_float_t exp_base,
                                                                         std_float_t iterative_epsilon,
                                                                         size_t step_count,
                                                                         size_t iteration_count,
                                                                         bool direction,
                                                                         std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& optimality_approximator) -> std::unique_ptr<TimeMachineOptimizerInterface>
            {
                if (std::isnan(exp_base))
                {
                    throw std::invalid_argument("bad exp_base, NaN");
                }

                return get_it_time_machine_optimizer(seed,
                                                     ExponentialDeltaStepFunction(exp_base),
                                                     iterative_epsilon,
                                                     step_count,
                                                     iteration_count,
                                                     direction,
                                                     std::move(optimality_approximator));
            }

            static auto get_linear_iterative_time_machine_optimizer(std_float_t seed,
                                                                    std_float_t a,
                                                                    std_float_t iterative_epsilon,
                                                                    size_t step_count,
                                                                    size_t iteration_count,
                                                                    bool direction,
                                                                    std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& optimality_approximator) -> std::unique_ptr<TimeMachineOptimizerInterface>
            {
                if (std::isnan(a))
                {
                    throw std::invalid_argument("bad slope, NaN");
                }

                return get_it_time_machine_optimizer(seed,
                                                     LinearDeltaStepFunction(a),
                                                     iterative_epsilon,
                                                     step_count,
                                                     iteration_count,
                                                     direction,
                                                     std::move(optimality_approximator));
            }

            static auto get_chaotic_iterative_time_machine_optimizer(std_float_t seed,
                                                                     std_float_t height,
                                                                     std_float_t iterative_epsilon,
                                                                     size_t step_count,
                                                                     size_t iteration_count,
                                                                     bool direction,
                                                                     std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& optimality_approximator) -> std::unique_ptr<TimeMachineOptimizerInterface>
            {
                if (std::isnan(height))
                {
                    throw std::invalid_argument("bad slope, NaN");
                }

                return get_it_time_machine_optimizer(seed,
                                                     ChaoticDeltaStepFunction(height),
                                                     iterative_epsilon,
                                                     step_count,
                                                     iteration_count,
                                                     direction,
                                                     std::move(optimality_approximator));
            }

            static auto get_time_machine_optimizer_set_optimizer(std::vector<std::unique_ptr<TimeMachineOptimizerInterface>>&& optimizer_set) -> std::unique_ptr<TimeMachineOptimizerInterface>
            {
                if (optimizer_set.empty())
                {
                    throw std::invalid_argument("bad optimizer set, empty");
                }

                for (const auto& optimizer: optimizer_set)
                {
                    if (optimizer == nullptr)
                    {
                        throw std::invalid_argument("bad optimizer, null optimizer");
                    }
                }

                return std::make_unique<TimeMachineOptimizerSetOptimizer>(std::move(optimizer_set));
            }
    };
}

#endif