#ifndef __MATRIX_STEERING_SUBSYSTEM_BY_STEP_OPTIMIZER_H__
#define __MATRIX_STEERING_SUBSYSTEM_BY_STEP_OPTIMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <random>
#include <functional>
#include <algorithm>
#include <general_definition/float_def.h>
#include <common_exception/cancellation_token.h>
#include "conventional_randomizer.h"
#include <memory>

namespace by_step_optimizer
{
    using namespace float_def;

    struct OptimizationResult
    {
        eval_float_t eval_value;
        std::shared_ptr<void> result;
    };

    class StepOptimizerInterface
    {
        public:

            virtual ~StepOptimizerInterface() noexcept = default;

            virtual auto step(const std::shared_ptr<void>& optimizable,
                              common_exception::CancellationTokenInterface& cancellation_token) -> OptimizationResult = 0;
    };

    class StepTensorInterface
    {
        public:

            virtual ~StepTensorInterface() noexcept = default;

            virtual auto get_optimizable() -> std::shared_ptr<void> = 0;
            virtual void feedback(const std::shared_ptr<void>& new_optimizable, double eval_value) = 0;
    };

    class StepRecommenderInterface
    {
        public:

            virtual ~StepRecommenderInterface() noexcept = default;

            virtual auto next_step() -> std::unique_ptr<StepTensorInterface> = 0;
    };
    
    class OptimizerInterface
    {
        public:

            virtual ~OptimizerInterface() noexcept = default;

            virtual auto optimize(const std::shared_ptr<void>& optimizable,
                                  common_exception::CancellationTokenInterface& cancellation_token) -> std::shared_ptr<void> = 0;
    };

    //according to the theory of convergence
    //the deviation(0) > deviation(1) > deviation(2)

    //the propagation wave of those is also adjusted accoridingly as we best_converge_next_step()
    //in the sense, we are pulling the deviation space, to achieve better convergence, this is achieved by nailing the best_deviation value in the space, until best_deviation == 0 means that there is nothing left to be done

    class BestConvergeStepRecommender: public virtual StepRecommenderInterface
    {
        private:

            struct InternalRecommendable
            {
                std::shared_ptr<void> optimizable;

                std::optional<double> old_score;
                std::optional<double> last_score;
                size_t no_progress_retry_count;
            };

            std::shared_ptr<std::vector<InternalRecommendable>> recommendable_vec;
            conventional_randomizer::RandomizerObject randomizer;
            size_t forward_scanner_idx;

        public:

            BestConvergeStepRecommender(const std::shared_ptr<void>& seed): recommendable_vec(),
                                                                            randomizer(),
                                                                            forward_scanner_idx(0u)
            {
                if (seed == nullptr)
                {
                    throw std::invalid_argument("bad seed, null");
                }

                this->recommendable_vec = std::make_shared<std::vector<InternalRecommendable>>();

                this->recommendable_vec->push_back
                (
                    InternalRecommendable
                    {
                        .optimizable                = seed,
                        .old_score                  = std::nullopt,
                        .last_score                 = std::nullopt,
                        .no_progress_retry_count    = 0u
                    }
                );
            }

            auto next_step() -> std::unique_ptr<StepTensorInterface>
            {
                if (this->randomizer.flip_a_coin())
                {
                    return this->get_best_converge_next_step();
                }
                else
                {
                    return this->get_forward_scanner_next_step();
                }
            }

        private:

            static auto get_recommendable_convergence_score(InternalRecommendable& recommendable) -> double
            {
                if (!recommendable.old_score.has_value())
                {
                    return 0;
                }

                if (!recommendable.last_score.has_value())
                {
                    return 0;
                }

                double tentative_score          = recommendable.old_score.value() - recommendable.last_score.value();
                const size_t MAX_HALF_LIFE_SZ   = 20;

                if (recommendable.no_progress_retry_count > MAX_HALF_LIFE_SZ)
                {
                    return 0;
                }

                double adjusted_score           = tentative_score / std::pow(1.2, recommendable.no_progress_retry_count);

                return adjusted_score;

            }

            auto get_best_converge_idx() -> size_t
            {
                if (this->recommendable_vec == nullptr)
                {
                    std::abort();
                }

                if (this->recommendable_vec->empty())
                {
                    std::abort();
                }

                size_t idx                  = 0u;
                std::optional<double> score = std::nullopt;

                for (size_t i = 1u; i < this->recommendable_vec->size(); ++i)
                {
                    double cand_score   = get_recommendable_convergence_score((*this->recommendable_vec)[i]);

                    if (!score.has_value())
                    {
                        idx     = (i - 1);
                        score   = cand_score;
                    }

                    if (cand_score > score.value())
                    {
                        idx     = (i - 1);
                        score   = cand_score;
                    }
                }

                return idx;
            }

            auto get_best_converge_next_step() -> std::unique_ptr<StepTensorInterface>
            {
                size_t idx  = this->get_best_converge_idx();

                return std::make_unique<InternalStepTensor>(this->recommendable_vec, idx);
            }

            auto get_forward_scanner_next_step() -> std::unique_ptr<StepTensorInterface>
            {
                if (this->recommendable_vec == nullptr)
                {
                    std::abort();
                }

                if (this->recommendable_vec->empty())
                {
                    std::abort();
                }

                size_t idx                  = this->forward_scanner_idx % this->recommendable_vec->size();
                this->forward_scanner_idx   += 1;
                this->forward_scanner_idx   %= this->recommendable_vec->size();

                return std::make_unique<InternalStepTensor>(this->recommendable_vec, idx);
            }

            class InternalStepTensor: public virtual StepTensorInterface
            {
                private:

                    std::shared_ptr<std::vector<InternalRecommendable>> recommendable_vec;
                    size_t recommendable_idx;
                    bool was_feedback_invoked;

                public:

                    InternalStepTensor(std::shared_ptr<std::vector<InternalRecommendable>> recommendable_vec,
                                       size_t recommendable_idx): recommendable_vec(std::move(recommendable_vec)),
                                                                  recommendable_idx(recommendable_idx),
                                                                  was_feedback_invoked(false){}

                    auto get_optimizable() -> std::shared_ptr<void>
                    {
                        if (this->recommendable_idx >= this->recommendable_vec->size())
                        {
                            std::abort();
                        }

                        return (*this->recommendable_vec)[this->recommendable_idx].optimizable;
                    }

                    void feedback(const std::shared_ptr<void>& new_optimizable,
                                  double score)
                    {
                        if (std::exchange(this->was_feedback_invoked, true))
                        {
                            return;
                        }

                        if (new_optimizable == nullptr)
                        {
                            return;
                        }

                        if (std::isnan(score))
                        {
                            return;
                        }

                        if (this->recommendable_idx >= this->recommendable_vec->size())
                        {
                            std::abort();
                        }

                        if (this->recommendable_idx + 1u == this->recommendable_vec->size())
                        {
                            this->recommendable_vec->push_back(InternalRecommendable
                            {
                                .optimizable                = new_optimizable,
                                .old_score                  = score,
                                .last_score                 = score,
                                .no_progress_retry_count    = 0u
                            });
                        }
                        else
                        {
                            InternalRecommendable& updating_entity      = (*this->recommendable_vec)[this->recommendable_idx + 1];

                            if (!updating_entity.old_score.has_value())
                            {
                                std::abort();
                            }

                            if (!updating_entity.last_score.has_value())
                            {
                                std::abort();                                
                            }

                            double competing_value  = updating_entity.last_score.value();

                            if (score < competing_value)
                            {
                                updating_entity.optimizable             = new_optimizable;
                                updating_entity.old_score               = updating_entity.last_score;
                                updating_entity.last_score              = score;
                                updating_entity.no_progress_retry_count = 0u;
                            }
                            else
                            {
                                updating_entity.no_progress_retry_count += 1;
                            }
                        }
                    }
            };
    };

    class StepRecommenderFactory
    {
        public:

            static auto get_best_step_recommender(const std::shared_ptr<void>& seed) -> std::unique_ptr<StepRecommenderInterface>
            {
                return std::make_unique<BestConvergeStepRecommender>(seed);
            }
    };

    class FiniteStepOptimizer: public virtual OptimizerInterface
    {
        private:

            std::unique_ptr<StepOptimizerInterface> step_optimizer;
            size_t step_count;

        public:

            FiniteStepOptimizer(std::unique_ptr<StepOptimizerInterface> step_optimizer_arg,
                                size_t step_count_arg)
            {
                if (step_optimizer_arg == nullptr)
                {
                    throw std::invalid_argument("bad step optimizer, null");
                }

                if (step_count_arg == 0u)
                {
                    throw std::invalid_argument("bad step count, 0");
                }

                this->step_optimizer    = std::move(step_optimizer_arg);
                this->step_count        = step_count_arg;
            }

            auto optimize(const std::shared_ptr<void>& optimizable,
                          common_exception::CancellationTokenInterface& cancellation_token) -> std::shared_ptr<void>
            {
                std::unique_ptr<StepRecommenderInterface> step_recommender      = StepRecommenderFactory::get_best_step_recommender(optimizable);

                std::shared_ptr<void> rs_optimizable                            = optimizable;
                std::optional<eval_float_t> rs_optimizable_deviation            = std::nullopt;

                for (size_t i = 0u; i < this->step_count; ++i)
                {
                    std::unique_ptr<StepTensorInterface> next_step              = step_recommender->next_step();
                    std::shared_ptr<void> step_optimizable                      = next_step->get_optimizable();
                    OptimizationResult optimization_result                      = this->step_optimizer->step(step_optimizable, cancellation_token);

                    if (!std::isnan(optimization_result.eval_value))
                    {
                        if (!rs_optimizable_deviation.has_value())
                        {
                            rs_optimizable           = optimization_result.result;
                            rs_optimizable_deviation = optimization_result.eval_value;
                        }

                        if (optimization_result.eval_value < rs_optimizable_deviation.value())
                        {
                            rs_optimizable           = optimization_result.result;
                            rs_optimizable_deviation = optimization_result.eval_value;
                        }
                    }

                    next_step->feedback(optimization_result.result, optimization_result.eval_value);
                }

                return rs_optimizable;
            }
    };

    class ComponentFactory
    {
        public:

            static auto get_finite_step_optimizer(std::unique_ptr<StepOptimizerInterface> step_optimizer,
                                                  size_t step_count) -> std::unique_ptr<OptimizerInterface>
            {
                return std::make_unique<FiniteStepOptimizer>(std::move(step_optimizer), step_count);
            }
    };
}

#endif