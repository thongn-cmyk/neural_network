#ifndef __TIME_MACHINE_OPTIMIZER_2_FACTORY_H__
#define __TIME_MACHINE_OPTIMIZER_2_FACTORY_H__

#include <iostream>
#include <stl_extension/stdx.h>
#include "conventional_randomizer.h"
#include "global_optimality_approximator.h"
#include "local_optimality_approximator.h"
#include "time_machine_optimizer_factory.h"
#include <general_definition/float_def.h>
#include "branch_optimizer.h"

namespace global_optimality_approximator
{
    using std_float_t = float_def::std_float_t;

    class DecisiveFactoryInterface
    {
        public:

            virtual ~DecisiveFactoryInterface() = default;

            virtual auto get_optimizer(const std::vector<size_t>& enumeration_vec) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> = 0;
            virtual auto get_enumeration_preorder_tree() -> std::vector<size_t> = 0;
    };

    class FactoryTensorInterface
    {
        public:

            virtual ~FactoryTensorInterface() = default;

            virtual auto get() -> std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> = 0;
            virtual void feedback(std_float_t score) = 0;
    };

    class TensorFactoryInterface
    {
        public:

            virtual ~TensorFactoryInterface() = default;
            virtual auto get() -> std::unique_ptr<FactoryTensorInterface> = 0;
    };

    template <class PromotedFloatType = std_float_t>
    class NormalDecisiveFactory: public virtual DecisiveFactoryInterface
    {
        private:

            struct Signature{};

            using ApplicationRandomizer = conventional_randomizer::ApplicationRandomizerFacility<Signature>;
            using Randomizer            = conventional_randomizer::RandomizerFacility<Signature>;

            static_assert(sizeof(PromotedFloatType) >= sizeof(double));

            static auto get_lnr_tm_high_focal_first_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double MIN_X_A            = 0.0000000001;
                const double MAX_X_A            = 0.1;   
                const double X_A                = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST   = 1u;
                const size_t STEP_COUNT_LAST    = 7u;

                double seed                     = ApplicationRandomizer::ld_randomize_focal();
                double step                     = ApplicationRandomizer::ld_randomize_focal();
                size_t step_count               = size_t{1} << Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_newton_naive_optimality_approximator<PromotedFloatType>(X_A));
            }

            static auto get_lnr_tm_high_focal_second_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double MIN_X_A            = 0.0000000001;
                const double MAX_X_A            = 0.1;
                const double X_A                = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST   = 1u;
                const size_t STEP_COUNT_LAST    = 7u;

                double seed                     = ApplicationRandomizer::ld_randomize_focal();
                double step                     = ApplicationRandomizer::ld_randomize_focal();
                size_t step_count               = size_t{1} << Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_newton_naive_optimality_approximator<PromotedFloatType>(X_A));
            }

            static auto get_lnr_tm_low_focal_first_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double MIN_X_A            = 0.0000000001;
                const double MAX_X_A            = 0.1;   
                const double X_A                = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_RELIABLE_X     = 0.0000000001;
                const double MAX_RELIABLE_X     = 1;
                const double RELIABLE_X         = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_STEP           = 0.001;
                const double MAX_STEP           = 1;

                const size_t STEP_COUNT_FIRST   = 1u;
                const size_t STEP_COUNT_LAST    = 7u;

                double seed                     = ApplicationRandomizer::ld_randomize_focal_2();
                double step                     = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal()), MIN_STEP, MAX_STEP);
                size_t step_count               = size_t{1} << Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(X_A, RELIABLE_X));
            }

            static auto get_lnr_tm_low_focal_second_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double MIN_X_A            = 0.0000000001;
                const double MAX_X_A            = 0.1;   
                const double X_A                = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_RELIABLE_X     = 0.0000000001;
                const double MAX_RELIABLE_X     = 1;
                const double RELIABLE_X         = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_STEP           = 0.001;
                const double MAX_STEP           = 1;

                const size_t STEP_COUNT_FIRST   = 1u;
                const size_t STEP_COUNT_LAST    = 7u;

                double seed                     = ApplicationRandomizer::ld_randomize_focal_2();
                double step                     = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal()), MIN_STEP, MAX_STEP);
                size_t step_count               = size_t{1} << Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(X_A, RELIABLE_X));
            }

            static auto get_exp_tm_high_focal_first_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 10;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_exponential_time_machine_optimizer(seed,
                                                                                 exp_base,
                                                                                 step_count,
                                                                                 local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_newton_naive_optimality_approximator<PromotedFloatType>(X_A));
            }

            static auto get_exp_tm_high_focal_second_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 10;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_exponential_time_machine_optimizer(seed,
                                                                                 exp_base,
                                                                                 step_count,
                                                                                 local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_newton_naive_optimality_approximator<PromotedFloatType>(X_A));
            }

            static auto get_exp_tm_low_focal_first_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 1;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;   
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_RELIABLE_X                 = 0.0000000001;
                const double MAX_RELIABLE_X                 = 1;
                const double RELIABLE_X                     = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal_2();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_exponential_time_machine_optimizer(seed,
                                                                                 exp_base,
                                                                                 step_count,
                                                                                 local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(X_A, RELIABLE_X));

            }

            static auto get_exp_tm_low_focal_second_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 1;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;   
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_RELIABLE_X                 = 0.0000000001;
                const double MAX_RELIABLE_X                 = 1;
                const double RELIABLE_X                     = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal_2();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_exponential_time_machine_optimizer(seed,
                                                                                 exp_base,
                                                                                 step_count,
                                                                                 local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(X_A, RELIABLE_X));
            }

            static auto get_iter_exp_tm_high_focal_first_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 10;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_ITERATIVE_EPSILON          = 0.0000000001;
                const double MAX_ITERATIVE_EPSILON          = 1;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = 7u;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_ITERATIVE_EPSILON, MAX_ITERATIVE_EPSILON);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();

                return RawMachineFactory::get_exponential_iterative_time_machine_optimizer(seed,
                                                                                           exp_base,
                                                                                           iterative_epsilon,
                                                                                           step_count,
                                                                                           iteration_count,
                                                                                           direction,
                                                                                           local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_newton_naive_optimality_approximator<PromotedFloatType>(X_A));
            }

            static auto get_iter_exp_tm_high_focal_second_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 10;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_ITERATIVE_EPSILON          = 0.0000000001;
                const double MAX_ITERATIVE_EPSILON          = 1;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 4;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = 7u;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_ITERATIVE_EPSILON, MAX_ITERATIVE_EPSILON);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();

                return RawMachineFactory::get_exponential_iterative_time_machine_optimizer(seed,
                                                                                           exp_base,
                                                                                           iterative_epsilon,
                                                                                           step_count,
                                                                                           iteration_count,
                                                                                           direction,
                                                                                           local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_newton_naive_optimality_approximator<PromotedFloatType>(X_A));
            }

            static auto get_iter_exp_tm_low_focal_first_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 1;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_ITERATIVE_EPSILON          = 0.0000000001;
                const double MAX_ITERATIVE_EPSILON          = 1;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;   
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_RELIABLE_X                 = 0.0000000001;
                const double MAX_RELIABLE_X                 = 1;
                const double RELIABLE_X                     = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 3;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = 7u;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal_2();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_ITERATIVE_EPSILON, MAX_ITERATIVE_EPSILON);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();

                return RawMachineFactory::get_exponential_iterative_time_machine_optimizer(seed,
                                                                                           exp_base,
                                                                                           iterative_epsilon,
                                                                                           step_count,
                                                                                           iteration_count,
                                                                                           direction,
                                                                                           local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(X_A, RELIABLE_X));

            }

            static auto get_iter_exp_tm_low_focal_second_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 1;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_ITERATIVE_EPSILON          = 0.0000000001;
                const double MAX_ITERATIVE_EPSILON          = 1;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;   
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_RELIABLE_X                 = 0.0000000001;
                const double MAX_RELIABLE_X                 = 1;
                const double RELIABLE_X                     = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 3;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = 7u;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal_2();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_ITERATIVE_EPSILON, MAX_ITERATIVE_EPSILON);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();

                return RawMachineFactory::get_exponential_iterative_time_machine_optimizer(seed,
                                                                                           exp_base,
                                                                                           iterative_epsilon,
                                                                                           step_count,
                                                                                           iteration_count,
                                                                                           direction,
                                                                                           local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(X_A, RELIABLE_X));
            }

            static auto get_iter_2_exp_tm_high_focal_first_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 10;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_ITERATIVE_EPSILON          = 0.0000000001;
                const double MAX_ITERATIVE_EPSILON          = 100;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 3;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = 7u;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal()), MIN_ITERATIVE_EPSILON, MAX_ITERATIVE_EPSILON);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();

                return RawMachineFactory::get_chaotic_iterative_time_machine_optimizer(seed,
                                                                                       exp_base,
                                                                                       iterative_epsilon,
                                                                                       step_count,
                                                                                       iteration_count,
                                                                                       direction,
                                                                                       local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_newton_naive_optimality_approximator<PromotedFloatType>(X_A));
            }

            static auto get_iter_2_exp_tm_high_focal_second_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 10;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_ITERATIVE_EPSILON          = 0.0000000001;
                const double MAX_ITERATIVE_EPSILON          = 100;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 3;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = 7u;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal()), MIN_ITERATIVE_EPSILON, MAX_ITERATIVE_EPSILON);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();

                return RawMachineFactory::get_chaotic_iterative_time_machine_optimizer(seed,
                                                                                       exp_base,
                                                                                       iterative_epsilon,
                                                                                       step_count,
                                                                                       iteration_count,
                                                                                       direction,
                                                                                       local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_newton_naive_optimality_approximator<PromotedFloatType>(X_A));
            }

            static auto get_iter_2_exp_tm_low_focal_first_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 1;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_ITERATIVE_EPSILON          = 0.0000000001;
                const double MAX_ITERATIVE_EPSILON          = 1;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;   
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_RELIABLE_X                 = 0.0000000001;
                const double MAX_RELIABLE_X                 = 1;
                const double RELIABLE_X                     = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 3;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = 7u;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal_2();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_ITERATIVE_EPSILON, MAX_ITERATIVE_EPSILON);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();

                return RawMachineFactory::get_chaotic_iterative_time_machine_optimizer(seed,
                                                                                       exp_base,
                                                                                       iterative_epsilon,
                                                                                       step_count,
                                                                                       iteration_count,
                                                                                       direction,
                                                                                       local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(X_A, RELIABLE_X));

            }

            static auto get_iter_2_exp_tm_low_focal_second_order() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                const double EXP_BASE_FIRST                 = 0.1;
                const double EXP_BASE_LAST                  = 1;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const double MIN_ITERATIVE_EPSILON          = 0.0000000001;
                const double MAX_ITERATIVE_EPSILON          = 1;

                const double MIN_X_A                        = 0.0000000001;
                const double MAX_X_A                        = 0.1;   
                const double X_A                            = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const double MIN_RELIABLE_X                 = 0.0000000001;
                const double MAX_RELIABLE_X                 = 1;
                const double RELIABLE_X                     = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_X_A, MAX_X_A);

                const size_t STEP_COUNT_FIRST               = 1u;
                const size_t STEP_COUNT_LAST                = size_t{1} << 3;

                const size_t ITERATION_COUNT_FIRST          = 1u;
                const size_t ITERATION_COUNT_LAST           = 7u;

                double seed                                 = ApplicationRandomizer::ld_randomize_focal_2();
                double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = stdx::float_clamp(static_cast<double>(ApplicationRandomizer::ld_randomize_focal_2()), MIN_ITERATIVE_EPSILON, MAX_ITERATIVE_EPSILON);
                size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << Randomizer::randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = Randomizer::flip_a_coin();

                return RawMachineFactory::get_chaotic_iterative_time_machine_optimizer(seed,
                                                                                       exp_base,
                                                                                       iterative_epsilon,
                                                                                       step_count,
                                                                                       iteration_count,
                                                                                       direction,
                                                                                       local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(X_A, RELIABLE_X));
            }

            static auto get_preorder_tree_from_graph(const std::unordered_map<std::string, std::vector<std::string>>& graph,
                                                     const std::string& origin) -> std::vector<size_t>
            {
                auto map_ptr = graph.find(origin);

                if (map_ptr == graph.end())
                {
                    return {0u};
                }

                std::vector<size_t> result{map_ptr->second.size()};

                for (const std::string& other_origin: map_ptr->second)
                {
                    std::vector<size_t> other = self::get_preorder_tree_from_graph(graph, other_origin);
                    std::copy(other.begin(), other.end(), std::back_inserter(result));
                }

                return result;
            }

        private:

            using self = NormalDecisiveFactory;

            static inline const std::unordered_map<std::string, std::vector<std::string>> DECISION_TREE =
            {
                {"origin", {"lnr_tm", "exp_tm", "iter_exp_tm", "iter_2_exp_tm"}},

                {"lnr_tm", {"lnr_tm_high_focal", "lnr_tm_low_focal"}},
                {"exp_tm", {"exp_tm_high_focal", "exp_tm_low_focal"}},
                {"iter_exp_tm", {"iter_exp_tm_high_focal", "iter_exp_tm_low_focal"}},
                {"iter_2_exp_tm", {"iter_2_exp_tm_high_focal", "iter_2_exp_tm_low_focal"}},

                {"lnr_tm_high_focal", {"lnr_tm_high_focal_first_order", "lnr_tm_high_focal_second_order"}},
                {"lnr_tm_low_focal", {"lnr_tm_low_focal_first_order", "lnr_tm_low_focal_second_order"}},
                {"exp_tm_high_focal", {"exp_tm_high_focal_first_order", "exp_tm_high_focal_second_order"}},
                {"exp_tm_low_focal", {"exp_tm_low_focal_first_order", "exp_tm_low_focal_second_order"}},
                {"iter_exp_tm_high_focal", {"iter_exp_tm_high_focal_first_order", "iter_exp_tm_high_focal_second_order"}},
                {"iter_exp_tm_low_focal", {"iter_exp_tm_low_focal_first_order", "iter_exp_tm_low_focal_second_order"}},
                {"iter_2_exp_tm_high_focal", {"iter_2_exp_tm_high_focal_first_order", "iter_2_exp_tm_high_focal_second_order"}},
                {"iter_2_exp_tm_low_focal", {"iter_2_exp_tm_low_focal_first_order", "iter_2_exp_tm_low_focal_second_order"}}
            };

            using factory_func_t = std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> (*) ();

            static inline const std::unordered_map<std::string, factory_func_t> LEAF_GRAPH =
            {
                {"lnr_tm_high_focal_first_order", self::get_lnr_tm_high_focal_first_order},
                {"lnr_tm_high_focal_second_order", self::get_lnr_tm_high_focal_second_order},
                {"lnr_tm_low_focal_first_order", self::get_lnr_tm_low_focal_first_order},
                {"lnr_tm_low_focal_second_order", self::get_lnr_tm_low_focal_second_order},

                {"exp_tm_high_focal_first_order", self::get_exp_tm_high_focal_first_order},
                {"exp_tm_high_focal_second_order", self::get_exp_tm_high_focal_second_order},
                {"exp_tm_low_focal_first_order", self::get_exp_tm_low_focal_first_order},
                {"exp_tm_low_focal_second_order", self::get_exp_tm_low_focal_second_order},

                {"iter_exp_tm_high_focal_first_order", self::get_iter_exp_tm_high_focal_first_order},
                {"iter_exp_tm_high_focal_second_order", self::get_iter_exp_tm_high_focal_second_order},
                {"iter_exp_tm_low_focal_first_order", self::get_iter_exp_tm_low_focal_first_order},
                {"iter_exp_tm_low_focal_second_order", self::get_iter_exp_tm_low_focal_second_order},

                {"iter_2_exp_tm_high_focal_first_order", self::get_iter_2_exp_tm_high_focal_first_order},
                {"iter_2_exp_tm_high_focal_second_order", self::get_iter_2_exp_tm_high_focal_second_order},
                {"iter_2_exp_tm_low_focal_first_order", self::get_iter_2_exp_tm_low_focal_first_order},
                {"iter_2_exp_tm_low_focal_second_order", self::get_iter_2_exp_tm_low_focal_second_order}
            };

            static inline const std::string ORIGIN = "origin";

        public:

            auto get_optimizer(const std::vector<size_t>& enumeration_vec) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                std::string leaf = ORIGIN;

                for (size_t enumeration: enumeration_vec)
                {
                    auto map_ptr = self::DECISION_TREE.find(leaf);

                    if (map_ptr == self::DECISION_TREE.end())
                    {
                        throw std::invalid_argument("bad enumeration, enumeration not found");
                    }

                    if (enumeration >= map_ptr->second.size())
                    {
                        throw std::invalid_argument("bad enumeration, enumeration out of bound");
                    }

                    leaf = map_ptr->second[enumeration];
                }

                auto map_ptr = self::LEAF_GRAPH.find(leaf);

                if (map_ptr == self::LEAF_GRAPH.end())
                {
                    throw std::invalid_argument("bad enumeration, short enumeration vec");
                }

                return (map_ptr->second)();
            }

            auto get_enumeration_preorder_tree() -> std::vector<size_t>
            {
                static const std::vector<size_t> result = self::get_preorder_tree_from_graph(self::DECISION_TREE, self::ORIGIN);

                return result;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class AdvancedDecisiveFactory: public virtual DecisiveFactoryInterface
    {
        private:

            static_assert(sizeof(PromotedFloatType) >= sizeof(double));

            using self = AdvancedDecisiveFactory;

            static inline const std::vector<std::string> OPTIMIZATION_MACHINE_CHOICE_VEC
            {
                "linear",
                "exponential",
                "iterative_exponential",
                "chaotic_iterative_exponential"
            };

            static inline const std::vector<std::string> STEP_CHOICE_VEC
            {
                "uniform_distribution_decimal_range",
                "uniform_distribution_low_range",
                "uniform_distribution_mid_range",
                "uniform_distribution_high_range",
                "exponential_distribution_decimal_range",
                "exponential_distribution_low_range",
                "exponential_distribution_mid_range",
                "exponential_distribution_high_range"
            };

            static inline const std::vector<std::string> SEED_CHOICE_VEC
            {
                "uniform_distribution_decimal_range",
                "uniform_distribution_low_range",
                "uniform_distribution_mid_range",
                "uniform_distribution_high_range",
                "exponential_distribution_decimal_range",
                "exponential_distribution_low_range",
                "exponential_distribution_mid_range",
                "exponential_distribution_high_range"
            };

            static inline const std::vector<std::string> LOCAL_OPTIMIZATION_CHOICE_VEC
            {
                "first_order_chaotic_short_range_decimal",
                "first_order_chaotic_short_range_mid",
                "first_order_chaotic_short_range_high",

                "first_order_converging_short_range_decimal",
                "first_order_converging_short_range_mid",
                "first_order_converging_short_range_high",

                "first_order_converging_short_range_and_slope_decimal",
                "first_order_converging_short_range_and_slope_mid",
                "first_order_converging_short_range_and_slope_high",

                "first_order_free_range",

                "second_order_chaotic_short_range_decimal",
                "second_order_chaotic_short_range_mid",
                "second_order_chaotic_short_range_high",

                "second_order_converging_short_range_decimal",
                "second_order_converging_short_range_mid",
                "second_order_converging_short_range_high",

                "second_order_converging_short_range_and_slope_decimal",
                "second_order_converging_short_range_and_slope_mid",
                "second_order_converging_short_range_and_slope_high",

                "second_order_free_range"
            };

            conventional_randomizer::ApplicationRandomizerObject app_randomizer;
            conventional_randomizer::RandomizerObject raw_randomizer;

            void to_suffix_tree(size_t idx, std::vector<size_t>& rs)
            {
                if (idx == 4u)
                {
                    rs.push_back(0u);
                    return;
                }

                size_t nxt_suffix_sz;

                switch (idx)
                {
                    case 0:
                    {
                        nxt_suffix_sz = OPTIMIZATION_MACHINE_CHOICE_VEC.size();
                        break;
                    }
                    case 1:
                    {
                        nxt_suffix_sz = STEP_CHOICE_VEC.size();
                        break;
                    }
                    case 2:
                    {
                        nxt_suffix_sz = SEED_CHOICE_VEC.size();
                        break;
                    }
                    case 3:
                    {
                        nxt_suffix_sz = LOCAL_OPTIMIZATION_CHOICE_VEC.size();
                        break;
                    }
                    default:
                    {
                        throw std::runtime_error("invalid index, out of bound access");
                    }
                }

                rs.push_back(nxt_suffix_sz);

                for (size_t i = 0u; i < nxt_suffix_sz; ++i)
                {
                    this->to_suffix_tree(idx + 1, rs);
                }
            }

        public:

            auto get_optimizer(const std::vector<size_t>& enumeration_vec) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                return this->get_helper_0(enumeration_vec);
            }

            auto get_enumeration_preorder_tree() -> std::vector<size_t>
            {
                std::vector<size_t> prefix_tree{};
                this->to_suffix_tree(0u, prefix_tree);

                return prefix_tree;
            }

        private:

            auto get_unfdst_decimalrange_linear_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double step                                 = this->get_unfdst_decimalrange_seed();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            std::move(local_approximator));
            }

            auto get_unfdst_lowrange_linear_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double step                                 = this->get_unfdst_lowrange_seed();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            std::move(local_approximator));
            }

            auto get_unfdst_midrange_linear_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double step                                 = this->get_unfdst_midrange_seed();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            std::move(local_approximator));
            }

            auto get_unfdst_highrange_linear_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double step                                 = this->get_unfdst_highrange_seed();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            std::move(local_approximator));
            }

            //

            auto get_expdst_decimalrange_linear_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double step                                 = this->get_expdst_decimalrange_seed();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            std::move(local_approximator));
            }

            auto get_expdst_lowrange_linear_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double step                                 = this->get_expdst_lowrange_seed();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            std::move(local_approximator));

            }

            auto get_expdst_midrange_linear_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double step                                 = this->get_expdst_midrange_seed();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            std::move(local_approximator));

            }

            auto get_expdst_highrange_linear_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double step                                 = this->get_expdst_highrange_seed();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_linear_time_machine_optimizer(seed,
                                                                            step,
                                                                            step_count,
                                                                            std::move(local_approximator));

            }

            //

            auto get_expdst_decimalrange_exponential_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const double EXP_BASE_FIRST                 = 0.000001f;
                const double EXP_BASE_LAST                  = 1.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double exp_base                             = this->raw_randomizer.randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_exponential_time_machine_optimizer(seed,
                                                                                 exp_base,
                                                                                 step_count,
                                                                                 std::move(local_approximator));
            }

            auto get_expdst_lowrange_exponential_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const double EXP_BASE_FIRST                 = 0.1f;
                const double EXP_BASE_LAST                  = 3.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double exp_base                             = this->raw_randomizer.randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_exponential_time_machine_optimizer(seed,
                                                                                 exp_base,
                                                                                 step_count,
                                                                                 std::move(local_approximator));

            }

            auto get_expdst_midrange_exponential_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const double EXP_BASE_FIRST                 = 0.1f;
                const double EXP_BASE_LAST                  = 6.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double exp_base                             = this->raw_randomizer.randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_exponential_time_machine_optimizer(seed,
                                                                                 exp_base,
                                                                                 step_count,
                                                                                 std::move(local_approximator));

            }

            auto get_expdst_highrange_exponential_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const double EXP_BASE_FIRST                 = 0.1f;
                const double EXP_BASE_LAST                  = 12.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                double exp_base                             = this->raw_randomizer.randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

                return RawMachineFactory::get_exponential_time_machine_optimizer(seed,
                                                                                 exp_base,
                                                                                 step_count,
                                                                                 std::move(local_approximator));
            }

            //

            auto get_unfdst_decimalrange_iterexp_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double a                                    = this->get_unfdst_decimalrange_seed();
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_linear_iterative_time_machine_optimizer(seed,
                                                                                      a,
                                                                                      iterative_epsilon,
                                                                                      step_count,
                                                                                      iteration_count,
                                                                                      direction,
                                                                                      std::move(local_approximator));
            }

            auto get_unfdst_lowrange_iterexp_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double a                                    = this->get_unfdst_lowrange_seed();
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_linear_iterative_time_machine_optimizer(seed,
                                                                                      a,
                                                                                      iterative_epsilon,
                                                                                      step_count,
                                                                                      iteration_count,
                                                                                      direction,
                                                                                      std::move(local_approximator));

            }

            auto get_unfdst_midrange_iterexp_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double a                                    = this->get_unfdst_midrange_seed();
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_linear_iterative_time_machine_optimizer(seed,
                                                                                      a,
                                                                                      iterative_epsilon,
                                                                                      step_count,
                                                                                      iteration_count,
                                                                                      direction,
                                                                                      std::move(local_approximator));

            }

            auto get_unfdst_highrange_iterexp_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double a                                    = this->get_unfdst_highrange_seed();
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_linear_iterative_time_machine_optimizer(seed,
                                                                                      a,
                                                                                      iterative_epsilon,
                                                                                      step_count,
                                                                                      iteration_count,
                                                                                      direction,
                                                                                      std::move(local_approximator));
            }

            //

            auto get_expdst_decimalrange_iterexp_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const double EXP_BASE_FIRST                 = 0.000001f;
                const double EXP_BASE_LAST                  = 1.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double exp_base                             = this->raw_randomizer.randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_exponential_iterative_time_machine_optimizer(seed,
                                                                                           exp_base,
                                                                                           iterative_epsilon,
                                                                                           step_count,
                                                                                           iteration_count,
                                                                                           direction,
                                                                                           std::move(local_approximator));
            }

            auto get_expdst_lowrange_iterexp_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const double EXP_BASE_FIRST                 = 0.1f;
                const double EXP_BASE_LAST                  = 3.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double exp_base                             = this->raw_randomizer.randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_exponential_iterative_time_machine_optimizer(seed,
                                                                                           exp_base,
                                                                                           iterative_epsilon,
                                                                                           step_count,
                                                                                           iteration_count,
                                                                                           direction,
                                                                                           std::move(local_approximator));
            }

            auto get_expdst_midrange_iterexp_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const double EXP_BASE_FIRST                 = 0.1f;
                const double EXP_BASE_LAST                  = 6.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double exp_base                             = this->raw_randomizer.randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_exponential_iterative_time_machine_optimizer(seed,
                                                                                           exp_base,
                                                                                           iterative_epsilon,
                                                                                           step_count,
                                                                                           iteration_count,
                                                                                           direction,
                                                                                           std::move(local_approximator));

            }

            auto get_expdst_highrange_iterexp_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }


                const double EXP_BASE_FIRST                 = 0.1f;
                const double EXP_BASE_LAST                  = 12.f;
                const size_t EXP_BASE_DISCRETIZATION_SZ     = 1'000'000'000'000ULL;

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double exp_base                             = this->raw_randomizer.randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_exponential_iterative_time_machine_optimizer(seed,
                                                                                           exp_base,
                                                                                           iterative_epsilon,
                                                                                           step_count,
                                                                                           iteration_count,
                                                                                           direction,
                                                                                           std::move(local_approximator));

            }

            //

            auto get_expdst_decimalrange_iterexp2_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double height                               = this->get_expdst_decimalrange_seed();
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_chaotic_iterative_time_machine_optimizer(seed,
                                                                                       height,
                                                                                       iterative_epsilon,
                                                                                       step_count,
                                                                                       iteration_count,
                                                                                       direction,
                                                                                       std::move(local_approximator));
            }

            auto get_expdst_lowrange_iterexp2_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }

                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double height                               = this->get_expdst_lowrange_seed();
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_chaotic_iterative_time_machine_optimizer(seed,
                                                                                       height,
                                                                                       iterative_epsilon,
                                                                                       step_count,
                                                                                       iteration_count,
                                                                                       direction,
                                                                                       std::move(local_approximator));

            }

            auto get_expdst_midrange_iterexp2_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }


                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double height                               = this->get_expdst_midrange_seed();
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_chaotic_iterative_time_machine_optimizer(seed,
                                                                                       height,
                                                                                       iterative_epsilon,
                                                                                       step_count,
                                                                                       iteration_count,
                                                                                       direction,
                                                                                       std::move(local_approximator));
            }

            auto get_expdst_highrange_iterexp2_optimizer(std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (std::isnan(seed))
                {
                    throw std::invalid_argument("bad seed, NaN");
                }

                if (local_approximator == nullptr)
                {
                    throw std::invalid_argument("bad local approximator, null");
                }


                const size_t STEP_COUNT_FIRST               = 0u;
                const size_t STEP_COUNT_LAST                = 5u;

                const size_t ITERATION_COUNT_FIRST          = 0u;
                const size_t ITERATION_COUNT_LAST           = 5u;

                double height                               = this->get_expdst_highrange_seed();
                double iterative_epsilon                    = this->app_randomizer.ld_randomize_focal_2();
                size_t step_count                           = size_t{1} << this->raw_randomizer.randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);
                size_t iteration_count                      = size_t{1} << this->raw_randomizer.randomize_uint(ITERATION_COUNT_FIRST, ITERATION_COUNT_LAST);
                bool direction                              = this->raw_randomizer.flip_a_coin();
            
                return RawMachineFactory::get_chaotic_iterative_time_machine_optimizer(seed,
                                                                                       height,
                                                                                       iterative_epsilon,
                                                                                       step_count,
                                                                                       iteration_count,
                                                                                       direction,
                                                                                       std::move(local_approximator));
            }

            //

            auto get_unfdst_decimalrange_seed() -> std_float_t
            {
                using operating_float_t                 = long double;

                const operating_float_t RANGE_FIRST     = -1;
                const operating_float_t RANGE_LAST      = 1;
                const size_t DISCRETIZATION_SZ          = 1'000'000'000'000'000'000ULL;

                return this->raw_randomizer.randomize_fixed_point_float(RANGE_FIRST, RANGE_LAST, DISCRETIZATION_SZ);
            }

            auto get_unfdst_lowrange_seed() -> std_float_t
            {
                using operating_float_t                 = long double;

                const operating_float_t RANGE_FIRST     = -100;
                const operating_float_t RANGE_LAST      = 100;
                const size_t DISCRETIZATION_SZ          = 1'000'000'000'000'000'000ULL;

                return this->raw_randomizer.randomize_fixed_point_float(RANGE_FIRST, RANGE_LAST, DISCRETIZATION_SZ);
            }

            auto get_unfdst_midrange_seed() -> std_float_t
            {
                using operating_float_t                 = long double;

                const operating_float_t RANGE_FIRST     = -1'000'000LL;
                const operating_float_t RANGE_LAST      = 1'000'000LL;
                const size_t DISCRETIZATION_SZ          = 1'000'000'000'000'000'000ULL;

                return this->raw_randomizer.randomize_fixed_point_float(RANGE_FIRST, RANGE_LAST, DISCRETIZATION_SZ);
            }

            auto get_unfdst_highrange_seed() -> std_float_t
            {
                using operating_float_t                 = long double;

                const operating_float_t RANGE_FIRST     = -1'000'000'000'000'000LL;
                const operating_float_t RANGE_LAST      = 1'000'000'000'000'000LL;
                const size_t DISCRETIZATION_SZ          = 1'000'000'000'000'000'000ULL;

                return this->raw_randomizer.randomize_fixed_point_float(RANGE_FIRST, RANGE_LAST, DISCRETIZATION_SZ);
            }

            //

            auto get_expdst_decimalrange_seed() -> std_float_t
            {
                return this->app_randomizer.ld_randomize_percentage_focal();
            }

            auto get_expdst_lowrange_seed() -> std_float_t
            {
                using operating_float_t                 = long double;

                const operating_float_t RANGE_FIRST     = 0;
                const operating_float_t RANGE_LAST      = 100 * (this->raw_randomizer.flip_a_coin() ? -1 : 1);

                return RANGE_FIRST + (RANGE_LAST - RANGE_FIRST) * this->app_randomizer.ld_randomize_percentage_focal();
            }

            auto get_expdst_midrange_seed() -> std_float_t
            {
                using operating_float_t                 = long double;

                const operating_float_t RANGE_FIRST     = 0;
                const operating_float_t RANGE_LAST      = 1'000'000LL * (this->raw_randomizer.flip_a_coin() ? -1 : 1);
                const size_t DISCRETIZATION_SZ          = 1'000'000'000'000'000'000ULL;

                return RANGE_FIRST + (RANGE_LAST - RANGE_FIRST) * this->app_randomizer.ld_randomize_percentage_focal();
            }

            auto get_expdst_highrange_seed() -> std_float_t
            {
                return this->app_randomizer.ld_randomize_focal(true);
            }

            //

            auto get_first_order_chaotic_short_range_decimal_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1});
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal_2();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_chaotic_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_first_order_chaotic_short_range_mid_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 4);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_chaotic_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_first_order_chaotic_short_range_high_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_chaotic_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            //

            auto get_first_order_converging_short_range_decimal_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1});
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal_2();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_converging_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_first_order_converging_short_range_mid_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 4);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_converging_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_first_order_converging_short_range_high_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_converging_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            //

            auto get_first_order_converging_short_range_and_slope_decimal_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1});
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal_2();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_converging_short_sight_and_slope_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_first_order_converging_short_range_and_slope_mid_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 4);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_converging_short_sight_and_slope_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_first_order_converging_short_range_and_slope_high_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_converging_short_sight_and_slope_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            //

            auto get_first_order_free_range_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST  = 1u;
                const size_t DECIMAL_POW_LAST   = 10u;
                size_t decimal_pow              = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a           = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                return local_optimality_approximator::OptimalityApproximatorFactory::get_first_order_newton_naive_optimality_approximator<PromotedFloatType>(x_a);
            }

            //

            auto get_second_order_chaotic_short_range_decimal_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);

                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());
                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1});
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal_2();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_chaotic_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_second_order_chaotic_short_range_mid_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);

                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());
                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 4);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_chaotic_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_second_order_chaotic_short_range_high_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);

                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());
                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_chaotic_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            //

            auto get_second_order_converging_short_range_decimal_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1});
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal_2();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_converging_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_second_order_converging_short_range_mid_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 4);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_converging_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_second_order_converging_short_range_high_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_converging_short_sight_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            //

            auto get_second_order_converging_short_range_and_slope_decimal_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1});
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal_2();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_converging_short_sight_and_slope_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_second_order_converging_short_range_and_slope_mid_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 4);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_converging_short_sight_and_slope_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            auto get_second_order_converging_short_range_and_slope_high_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST          = 1u;
                const size_t DECIMAL_POW_LAST           = 10u;
                size_t decimal_pow                      = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a                   = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                const std_float_t min_deviation         = std::numeric_limits<std_float_t>::min();
                const std_float_t max_deviation         = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 18);
                const std_float_t tentative_deviation   = this->app_randomizer.ld_randomize_focal();
                const std_float_t deviation             = std::clamp(tentative_deviation, min_deviation, max_deviation);

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_converging_short_sight_and_slope_newton_naive_optimality_approximator<PromotedFloatType>(x_a, deviation);
            }

            //

            auto get_second_order_free_range_optimizer() -> std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>
            {
                const size_t DECIMAL_POW_FIRST  = 1u;
                const size_t DECIMAL_POW_LAST   = 10u;
                size_t decimal_pow              = this->raw_randomizer.randomize_uint(DECIMAL_POW_FIRST, DECIMAL_POW_LAST);
                const std_float_t x_a           = std::max(std::pow(std_float_t{10}, -static_cast<std_float_t>(decimal_pow)), std::numeric_limits<std_float_t>::min());

                return local_optimality_approximator::OptimalityApproximatorFactory::get_second_order_newton_naive_optimality_approximator<PromotedFloatType>(x_a);
            }

            //

            auto get_helper_0(const std::vector<size_t>& enumeration_vec) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (enumeration_vec.size() < 2)
                {
                    throw std::invalid_argument("bad enumeration vec, insufficient enumeration width");
                }

                auto generator = [&](std_float_t seed, std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator)
                {
                    size_t first    = enumeration_vec[0];
                    size_t second   = enumeration_vec[1];

                    if (first >= OPTIMIZATION_MACHINE_CHOICE_VEC.size())
                    {
                        throw std::invalid_argument("bad enumeration, enumeration out of range");
                    }

                    if (second >= STEP_CHOICE_VEC.size())
                    {
                        throw std::invalid_argument("bad enumeration, enumeration out of range");
                    }

                    if (OPTIMIZATION_MACHINE_CHOICE_VEC[first] == "linear")
                    {
                        if (STEP_CHOICE_VEC[second] == "uniform_distribution_decimal_range")
                        {
                            return this->get_unfdst_decimalrange_linear_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_low_range")
                        {
                            return this->get_unfdst_lowrange_linear_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_mid_range")
                        {
                            return this->get_unfdst_midrange_linear_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_high_range")
                        {
                            return this->get_unfdst_highrange_linear_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_decimal_range")
                        {
                            return this->get_expdst_decimalrange_linear_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_low_range")
                        {
                            return this->get_expdst_lowrange_linear_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_mid_range")
                        {
                            return this->get_expdst_midrange_linear_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_high_range")
                        {
                            return this->get_expdst_highrange_linear_optimizer(seed, std::move(local_approximator));
                        }
                        else
                        {
                            std::abort();
                        }
                    }
                    else if (OPTIMIZATION_MACHINE_CHOICE_VEC[first] == "exponential")
                    {
                        if (STEP_CHOICE_VEC[second] == "uniform_distribution_decimal_range")
                        {
                            return this->get_expdst_decimalrange_exponential_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_low_range")
                        {
                            return this->get_expdst_lowrange_exponential_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_mid_range")
                        {
                            return this->get_expdst_midrange_exponential_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_high_range")
                        {
                            return this->get_expdst_highrange_exponential_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_decimal_range")
                        {
                            return this->get_expdst_decimalrange_exponential_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_low_range")
                        {
                            return this->get_expdst_lowrange_exponential_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_mid_range")
                        {
                            return this->get_expdst_midrange_exponential_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_high_range")
                        {
                            return this->get_expdst_highrange_exponential_optimizer(seed, std::move(local_approximator));
                        }
                        else
                        {
                            std::abort();
                        }
                    }
                    else if (OPTIMIZATION_MACHINE_CHOICE_VEC[first] == "iterative_exponential")
                    {
                        if (STEP_CHOICE_VEC[second] == "uniform_distribution_decimal_range")
                        {
                            return this->get_unfdst_decimalrange_iterexp_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_low_range")
                        {
                            return this->get_unfdst_lowrange_iterexp_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_mid_range")
                        {
                            return this->get_unfdst_midrange_iterexp_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_high_range")
                        {
                            return this->get_unfdst_highrange_iterexp_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_decimal_range")
                        {
                            return this->get_expdst_decimalrange_iterexp_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_low_range")
                        {
                            return this->get_expdst_lowrange_iterexp_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_mid_range")
                        {
                            return this->get_expdst_midrange_iterexp_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_high_range")
                        {
                            return this->get_expdst_highrange_iterexp_optimizer(seed, std::move(local_approximator));
                        }
                        else
                        {
                            std::abort();
                        }
                    }
                    else if (OPTIMIZATION_MACHINE_CHOICE_VEC[first] == "chaotic_iterative_exponential")
                    {
                        if (STEP_CHOICE_VEC[second] == "uniform_distribution_decimal_range")
                        {
                            return this->get_expdst_decimalrange_iterexp2_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_low_range")
                        {
                            return this->get_expdst_lowrange_iterexp2_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_mid_range")
                        {
                            return this->get_expdst_midrange_iterexp2_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "uniform_distribution_high_range")
                        {
                            return this->get_expdst_highrange_iterexp2_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_decimal_range")
                        {
                            return this->get_expdst_decimalrange_iterexp2_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_low_range")
                        {
                            return this->get_expdst_lowrange_iterexp2_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_mid_range")
                        {
                            return this->get_expdst_midrange_iterexp2_optimizer(seed, std::move(local_approximator));
                        }
                        else if (STEP_CHOICE_VEC[second] == "exponential_distribution_high_range")
                        {
                            return this->get_expdst_highrange_iterexp2_optimizer(seed, std::move(local_approximator));
                        }
                        else
                        {
                            std::abort();
                        }
                    }
                    else
                    {
                        std::abort();
                    }
                };

                return this->get_helper_1({std::next(enumeration_vec.begin(), 2u), enumeration_vec.end()}, generator);
            }

            template <class Generator>
            auto get_helper_1(const std::vector<size_t>& enumeration_vec, Generator generator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (enumeration_vec.size() == 0u)
                {
                    throw std::invalid_argument("bad enumeration_vec, insufficient enumeration width");
                }

                auto other_generator = [&](std::unique_ptr<local_optimality_approximator::OptimalityApproximatorInterface>&& local_approximator)
                {
                    size_t front_value = enumeration_vec.front();

                    if (front_value >= SEED_CHOICE_VEC.size())
                    {
                        throw std::invalid_argument("bad enumeration, enumeration out of range");
                    }

                    if (SEED_CHOICE_VEC[front_value] == "uniform_distribution_decimal_range")
                    {
                        return generator(this->get_unfdst_decimalrange_seed(), std::move(local_approximator));
                    }
                    else if (SEED_CHOICE_VEC[front_value] == "uniform_distribution_low_range")
                    {
                        return generator(this->get_unfdst_lowrange_seed(), std::move(local_approximator));
                    }
                    else if (SEED_CHOICE_VEC[front_value] == "uniform_distribution_mid_range")
                    {
                        return generator(this->get_unfdst_midrange_seed(), std::move(local_approximator));
                    }
                    else if (SEED_CHOICE_VEC[front_value] == "uniform_distribution_high_range")
                    {
                        return generator(this->get_unfdst_highrange_seed(), std::move(local_approximator));
                    }
                    else if (SEED_CHOICE_VEC[front_value] == "exponential_distribution_decimal_range")
                    {
                        return generator(this->get_expdst_decimalrange_seed(), std::move(local_approximator));
                    }
                    else if (SEED_CHOICE_VEC[front_value] == "exponential_distribution_low_range")
                    {
                        return generator(this->get_expdst_lowrange_seed(), std::move(local_approximator));
                    }
                    else if (SEED_CHOICE_VEC[front_value] == "exponential_distribution_mid_range")
                    {
                        return generator(this->get_expdst_midrange_seed(), std::move(local_approximator));
                    }
                    else if (SEED_CHOICE_VEC[front_value] == "exponential_distribution_high_range")
                    {
                        return generator(this->get_expdst_highrange_seed(), std::move(local_approximator));
                    }
                    else
                    {
                        std::abort();
                    }
                };

                return this->get_helper_2({std::next(enumeration_vec.begin()), enumeration_vec.end()}, other_generator);
            }

            template <class Generator>
            auto get_helper_2(const std::vector<size_t>& enumeration_vec, Generator generator) -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                if (enumeration_vec.size() != 1u)
                {
                    throw std::invalid_argument("bad enumeration_vec, mismatched enumeration width");
                }

                size_t front_value = enumeration_vec.front();

                if (front_value >= LOCAL_OPTIMIZATION_CHOICE_VEC.size())
                {
                    throw std::invalid_argument("bad enumeration, enumeration out of range");
                }

                if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_chaotic_short_range_decimal")
                {
                    return generator(this->get_first_order_chaotic_short_range_decimal_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_chaotic_short_range_mid")
                {
                    return generator(this->get_first_order_chaotic_short_range_mid_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_chaotic_short_range_high")
                {
                    return generator(this->get_first_order_chaotic_short_range_high_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_converging_short_range_decimal")
                {
                    return generator(this->get_first_order_converging_short_range_decimal_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_converging_short_range_mid")
                {
                    return generator(this->get_first_order_converging_short_range_mid_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_converging_short_range_high")
                {
                    return generator(this->get_first_order_converging_short_range_high_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_converging_short_range_and_slope_decimal")
                {
                    return generator(this->get_first_order_converging_short_range_and_slope_decimal_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_converging_short_range_and_slope_mid")
                {
                    return generator(this->get_first_order_converging_short_range_and_slope_mid_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_converging_short_range_and_slope_high")
                {
                    return generator(this->get_first_order_converging_short_range_and_slope_high_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "first_order_free_range")
                {
                    return generator(this->get_first_order_free_range_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_chaotic_short_range_decimal")
                {
                    return generator(this->get_second_order_chaotic_short_range_decimal_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_chaotic_short_range_mid")
                {
                    return generator(this->get_second_order_chaotic_short_range_mid_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_chaotic_short_range_high")
                {
                    return generator(this->get_second_order_chaotic_short_range_high_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_converging_short_range_decimal")
                {
                    return generator(this->get_second_order_converging_short_range_decimal_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_converging_short_range_mid")
                {
                    return generator(this->get_second_order_converging_short_range_mid_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_converging_short_range_high")
                {
                    return generator(this->get_second_order_converging_short_range_high_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_converging_short_range_and_slope_decimal")
                {
                    return generator(this->get_second_order_converging_short_range_and_slope_decimal_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_converging_short_range_and_slope_mid")
                {
                    return generator(this->get_second_order_converging_short_range_and_slope_mid_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_converging_short_range_and_slope_high")
                {
                    return generator(this->get_second_order_converging_short_range_and_slope_high_optimizer());
                }
                else if (LOCAL_OPTIMIZATION_CHOICE_VEC[front_value] == "second_order_free_range")
                {
                    return generator(this->get_second_order_free_range_optimizer());
                }
                else
                {
                    std::abort();
                }
            }
    };

    class TensorFactory: public virtual TensorFactoryInterface
    {
        private:

            std::unique_ptr<DecisiveFactoryInterface> decisive_factory;
            std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor;

        public:

            TensorFactory(std::unique_ptr<DecisiveFactoryInterface> decisive_factory,
                          std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor) noexcept: decisive_factory(std::move(decisive_factory)),
                                                                                                                          branch_predictor(std::move(branch_predictor)){}
            
            auto get() -> std::unique_ptr<FactoryTensorInterface>
            {
                std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch_prediction_result = this->branch_predictor->next();
                std::vector<size_t> enumeration_vec = branch_prediction_result->get_enumeration();
                std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> time_machine         = this->decisive_factory->get_optimizer(enumeration_vec);

                return std::make_unique<InternalFactoryTensor>
                (
                    std::move(time_machine),
                    std::move(branch_prediction_result)
                );
            }
        
        private:
            
            class InternalFactoryTensor: public virtual FactoryTensorInterface
            {
                private:

                    std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> time_machine;
                    std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch_prediction_result;

                public:

                    InternalFactoryTensor(std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> time_machine,
                                          std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch_prediction_result) noexcept: time_machine(std::move(time_machine)),
                                                                                                                                                         branch_prediction_result(std::move(branch_prediction_result)){}

                    auto get() -> std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
                    {
                        return this->time_machine;
                    }

                    void feedback(std_float_t score)
                    {
                        this->branch_prediction_result->feedback(score);
                    }
            };
    };

    template <class PromotedFloatType = std_float_t>
    class TraditionalTensorFactory: public virtual TensorFactoryInterface
    {
        public:

            auto get() -> std::unique_ptr<FactoryTensorInterface>
            {
                return std::make_unique<InternalFactoryTensor>(global_optimality_approximator::TimeMachineOptimizerFactory::get_random_taylor_time_machine_optimizer<PromotedFloatType>());
            }
        
        private:
            
            class InternalFactoryTensor: public virtual FactoryTensorInterface
            {
                private:

                    std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> time_machine;

                public:

                    InternalFactoryTensor(std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> time_machine) noexcept: time_machine(std::move(time_machine)){}

                    auto get() -> std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
                    {
                        return this->time_machine;
                    }

                    void feedback(std_float_t score)
                    {
                        (void) score;
                    }
            };
    };
    
    class ProbabilisticTensorFactory: public virtual TensorFactoryInterface
    {
        private:

            std::unique_ptr<TensorFactoryInterface> lhs;
            std::unique_ptr<TensorFactoryInterface> rhs;
            conventional_randomizer::ChanceMachine randomizer;
        
        public:

            ProbabilisticTensorFactory(std::unique_ptr<TensorFactoryInterface> lhs,
                                       std::unique_ptr<TensorFactoryInterface> rhs,
                                       conventional_randomizer::ChanceMachine randomizer): lhs(std::move(lhs)),
                                                                                           rhs(std::move(rhs)),
                                                                                           randomizer(std::move(randomizer)){}

            auto get() -> std::unique_ptr<FactoryTensorInterface>
            {
                if (this->randomizer.flip_a_coin())
                {
                    return this->lhs->get();
                }
                else
                {
                    return this->rhs->get();
                }
            }
    };

    class TensorFactoryFactory
    {
        public:

            template <class PromotedFloatType = std_float_t>
            static auto get_normal_factory() -> std::unique_ptr<TensorFactoryInterface>
            {
                std::unique_ptr<DecisiveFactoryInterface> decisive_factory                              = std::make_unique<NormalDecisiveFactory<PromotedFloatType>>();
                std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor    = branch_optimizer::HierarchicalBranchPredictorFactory::get_best_branch_predictor_from_preorder_tree(decisive_factory->get_enumeration_preorder_tree());

                return std::make_unique<TensorFactory>(std::move(decisive_factory),
                                                       std::move(branch_predictor));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_advanced_factory() -> std::unique_ptr<TensorFactoryInterface>
            {
                std::unique_ptr<DecisiveFactoryInterface> decisive_factory                              = std::make_unique<AdvancedDecisiveFactory<PromotedFloatType>>();
                std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor    = branch_optimizer::HierarchicalBranchPredictorFactory::get_best_branch_predictor_from_preorder_tree(decisive_factory->get_enumeration_preorder_tree());

                return std::make_unique<TensorFactory>(std::move(decisive_factory),
                                                       std::move(branch_predictor));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_traditional_factory() -> std::unique_ptr<TensorFactoryInterface>
            {
                return std::make_unique<TraditionalTensorFactory<PromotedFloatType>>();
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_best_factory() -> std::unique_ptr<TensorFactoryInterface>
            {
                const size_t RANDOM_CHANCE_NUM      = 3u;
                const size_t RANDOM_CHANCE_DENOM    = 10u;

                return std::make_unique<ProbabilisticTensorFactory>(get_traditional_factory(),
                                                                    get_advanced_factory(),
                                                                    conventional_randomizer::ChanceMachine(RANDOM_CHANCE_DENOM, RANDOM_CHANCE_NUM));
            }
    };
}

#endif