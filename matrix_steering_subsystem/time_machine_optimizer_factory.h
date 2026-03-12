//HEADER_CONTROL 4

#ifndef __TIME_MACHINE_OPTIMIZER_FACTORY_H__
#define __TIME_MACHINE_OPTIMIZER_FACTORY_H__

#include <stl_extension/stdx.h>
#include "conventional_randomizer.h"
#include "global_optimality_approximator.h"
#include "local_optimality_approximator.h"
#include <general_definition/float_def.h>

namespace global_optimality_approximator
{
    using std_float_t = float_def::std_float_t;

    //we have done well for the root approximation, we'd try to write the tests for roots finding tomorrow, followed by an exponential change to the equation
    //we'd try to work on this ASAP
    //we have completed the root finder component, the function is expected to work properly for all range of inputs, as long as the time_machine is stable

    //the next component is the transportation component, where we'd want to exponentially increase the influences of the transporation logits on the suceeding ground logits
    //we can either, (1): have dedicated update slots for certain transportation layer
    //increase the number of rotation, and actually rotate a virtual matrix to achieved said goals

    class TimeMachineOptimizerFactory
    {
        private:

            struct Signature{};

            using Randomizer            = conventional_randomizer::RandomizerFacility<Signature>;
            using ApplicationRandomizer = conventional_randomizer::ApplicationRandomizerFacility<Signature>;

            template <class PromotedFloatType = std_float_t>
            static auto get_random_first_order_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST  = 1u;
                const size_t DECIMAL_POW_LAST   = 10u;
                size_t decimal_pow              = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a           = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_newton_naive_optimality_approximator<PromotedFloatType>(x_a);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_first_order_ss_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = ApplicationRandomizer::ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_first_order_css_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = ApplicationRandomizer::ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_chaotic_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_first_order_css2_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = ApplicationRandomizer::ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_converging_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_first_order_css3_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = ApplicationRandomizer::ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_converging_short_sight_and_slope_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_second_order_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST  = 1u;
                const size_t DECIMAL_POW_LAST   = 10u;
                size_t decimal_pow              = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a           = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_newton_naive_optimality_approximator<PromotedFloatType>(x_a);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_second_order_ss_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);

                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());
                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = ApplicationRandomizer::ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_second_order_css_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);

                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());
                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = ApplicationRandomizer::ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_chaotic_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_second_order_css2_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);

                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());
                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = ApplicationRandomizer::ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_converging_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_second_order_css3_newton_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = Randomizer::randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);

                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());
                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = ApplicationRandomizer::ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_converging_short_sight_and_slope_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_local_optimality_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t ENUMERATION_SZ     = 10u;
                const size_t optimizer_idx      = Randomizer::randomize_uint(0u, ENUMERATION_SZ);

                switch (optimizer_idx)
                {
                    case 0:
                    {
                        return get_random_first_order_newton_optimizer<PromotedFloatType>();
                    }
                    case 1:
                    {
                        return get_random_first_order_ss_newton_optimizer<PromotedFloatType>();
                    }
                    case 2:
                    {
                        return get_random_first_order_css_newton_optimizer<PromotedFloatType>();
                    }
                    case 3:
                    {
                        return get_random_second_order_newton_optimizer<PromotedFloatType>();
                    }
                    case 4:
                    {
                        return get_random_second_order_ss_newton_optimizer<PromotedFloatType>();
                    }
                    case 5:
                    {
                        return get_random_second_order_css_newton_optimizer<PromotedFloatType>();
                    }
                    case 6:
                    {
                        return get_random_first_order_css2_newton_optimizer<PromotedFloatType>();
                    }
                    case 7:
                    {
                        return get_random_second_order_css_newton_optimizer<PromotedFloatType>();
                    }
                    case 8:
                    {
                        return get_random_first_order_css3_newton_optimizer<PromotedFloatType>();
                    }
                    case 9:
                    {
                        return get_random_second_order_css3_newton_optimizer<PromotedFloatType>();
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

        public:

            template <class PromotedFloatType = std_float_t>
            static auto get_random_taylor_linear_time_machine_optimizer() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal(true);
                double step                                 = ApplicationRandomizer::ld_randomize_focal(true);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            get_random_local_optimality_optimizer<PromotedFloatType>());
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_taylor_exponential_time_machine_optimizer() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const double EXP_BASE_FIRST                 = 0.1f;
                const double EXP_BASE_LAST                  = 10.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal(true);
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_exponential_time_machine_optimizer(seed,
                                                                                 exp_base,
                                                                                 step_count,
                                                                                 get_random_local_optimality_optimizer<PromotedFloatType>());
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_taylor_exponential_iterative_time_machine_optimizer() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const double EXP_BASE_FIRST                 = 0.1f;
                const double EXP_BASE_LAST                  = 10.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = size_t{1} << 4;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal(true);
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = ApplicationRandomizer::ld_randomize_focal_2();
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();
            
                return RawMachineFactory::get_exponential_iterative_time_machine_optimizer(seed,
                                                                                           exp_base,
                                                                                           iterative_epsilon,
                                                                                           step_count,
                                                                                           iteration_count,
                                                                                           direction,
                                                                                           get_random_local_optimality_optimizer<PromotedFloatType>());
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_taylor_chaotic_iterative_time_machine_optimizer() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const double EXP_BASE_FIRST                 = 0.1f;
                const double EXP_BASE_LAST                  = 10.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = size_t{1} << 4;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal(true);
                double range                                = ApplicationRandomizer::ld_randomize_focal();
                double iterative_epsilon                    = ApplicationRandomizer::ld_randomize_focal_2();
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();

                return RawMachineFactory::get_chaotic_iterative_time_machine_optimizer(seed,
                                                                                       range,
                                                                                       iterative_epsilon,
                                                                                       step_count,
                                                                                       iteration_count,
                                                                                       direction,
                                                                                       get_random_local_optimality_optimizer<PromotedFloatType>());
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_taylor_time_machine_optimizer() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const size_t ENUMERATION_SZ = 4u;
                size_t dispatch_code        = Randomizer::randomize_uint(0u, ENUMERATION_SZ);

                switch (dispatch_code)
                {
                    case 0:
                    {
                        return get_random_taylor_linear_time_machine_optimizer<PromotedFloatType>();
                    }
                    case 1:
                    {
                        return get_random_taylor_exponential_time_machine_optimizer<PromotedFloatType>();
                    }
                    case 2:
                    {
                        return get_random_taylor_exponential_iterative_time_machine_optimizer<PromotedFloatType>();
                    }
                    case 3:
                    {
                        return get_random_taylor_chaotic_iterative_time_machine_optimizer<PromotedFloatType>();
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }
    };
}

#endif