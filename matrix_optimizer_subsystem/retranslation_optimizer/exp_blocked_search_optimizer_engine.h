#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_RETRANSLATION_OPTIMIZER_EXP_BLOCKED_SEARCH_OPTIMIZER_ENGINE_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_RETRANSLATION_OPTIMIZER_EXP_BLOCKED_SEARCH_OPTIMIZER_ENGINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <random>
#include <functional>
#include <algorithm>
#include <memory>
#include <matrix_optimizer_subsystem/matrix_optimizer_engine_interface.h>
#include <matrix/tensor_model.h>
#include <matrix/the_matrix_interface.h>
#include <common_exception/cancellation_token.h>
#include <common_exception/common_exception.h>
#include <vector>
#include "blocked_search_optimizer_engine.h"
#include <cmath>
#include <random>
#include <utility>
#include <stl_extension/stdx.h>
#include <bit>
#include <stl_extension/hasher.h>

namespace matrix_optimizer_subsystem
{
    // using namespace float_def;

    //move
    class ExponentialDistributionRangeRandomizer
    {
        private:

            using randomizer_t  = decltype(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64(uint32_t{})));

            randomizer_t randomizer;
            size_t range_sz;
            double dist_decay_rate;
            std::vector<double> distribution_vec;

            static inline constexpr double DIST_DECAY_RATE_MIN          = 1.2;
            static inline constexpr double DIST_DECAY_RATE_MAX          = 10;
            static inline constexpr double DIST_DECAY_RATE_DEFAULT      = 2;
            static inline constexpr size_t UNIFORM_DISTRIBUTION_RANGE   = size_t{1} << 30;

            static auto make_distribution_vector(size_t range_sz,
                                                 double dist_decay_rate) -> std::vector<double>
            {
                const double INITIAL_SCORE  = 1;
                double current_score        = 1;

                if (std::isnan(dist_decay_rate))
                {
                    throw std::invalid_argument("bad dist decay rate, NaN");
                }

                if (dist_decay_rate < 1)
                {
                    throw std::invalid_argument("bad dist decay rate, < 1");
                }

                std::vector<double> score_table{};

                for (size_t i = 0u; i < range_sz; ++i)
                {
                    score_table.push_back(current_score);
                    current_score /= dist_decay_rate;
                }
                
                double total_score = 0;

                for (size_t i = 0u; i < range_sz; ++i)
                {
                    total_score += score_table[i];
                }

                std::vector<double> norm_table{};

                for (size_t i = 0u; i < range_sz; ++i)
                {
                    double cand = score_table[i] / total_score;

                    if (std::isnan(cand))
                    {
                        cand = 0;
                    }

                    if (cand < 0)
                    {
                        cand = 0;
                    }

                    norm_table.push_back(cand);
                }

                std::vector<double> prob_table{};
                double current_prob = 0;
            
                for (size_t i = 0u; i < range_sz; ++i)
                {
                    prob_table.push_back(current_prob);
                    current_prob += norm_table[i];
                }

                return prob_table;
            }

            static auto randomization_seed() -> uint32_t
            {
                static std::atomic<size_t> random_counter{};

                size_t counter_clue = random_counter.fetch_add(1u, std::memory_order_relaxed);
                size_t stack_clue   = std::bit_cast<uint64_t>(static_cast<const void *>(&counter_clue));
                size_t time_clue    = static_cast<size_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
                auto clue           = std::make_tuple(counter_clue, stack_clue, time_clue);

                return hasher::hash_reflectible(clue);
            }

        public:

            ExponentialDistributionRangeRandomizer(size_t range_sz_arg,
                                                   double dist_decay_rate_arg): randomizer(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{randomization_seed()}))
            {
                if (range_sz_arg == 0u)
                {
                    throw std::invalid_argument("bad range, 0");
                }

                this->range_sz = range_sz_arg;

                if (std::isnan(dist_decay_rate_arg))
                {
                    dist_decay_rate_arg = DIST_DECAY_RATE_MIN;
                }

                dist_decay_rate_arg     = std::clamp(dist_decay_rate_arg, DIST_DECAY_RATE_MIN, DIST_DECAY_RATE_MAX);

                this->range_sz          = range_sz_arg;
                this->dist_decay_rate   = dist_decay_rate_arg;
                this->distribution_vec  = make_distribution_vector(range_sz_arg, dist_decay_rate_arg);
            }

            ExponentialDistributionRangeRandomizer(size_t range_sz_arg): ExponentialDistributionRangeRandomizer(range_sz_arg,
                                                                                                                DIST_DECAY_RATE_DEFAULT){}

            auto randomize() -> size_t
            {
                size_t idx  = this->randomizer() % UNIFORM_DISTRIBUTION_RANGE;
                double clue = static_cast<double>(idx) / UNIFORM_DISTRIBUTION_RANGE;
                
                return this->binary_search_enumeration(clue);
            }

        private:

            auto internal_lower_bound_prob_find(double prob, size_t first, size_t last) -> size_t
            {
                if (first + 2 == last)
                {
                    return first;
                }

                size_t range_sz     = last - first;
                size_t mid_sz       = range_sz / 2;
                size_t mid_point    = first + mid_sz;

                double cand = this->distribution_vec[mid_point];

                if (cand > prob)
                {
                    return this->internal_lower_bound_prob_find(prob, first, mid_point + 1);
                }

                return this->internal_lower_bound_prob_find(prob, mid_point, last);
            }

            auto binary_search_enumeration(double prob) -> size_t
            {
                if (std::isnan(prob))
                {
                    std::abort();
                }

                if (prob < 0)
                {
                    prob = 0;
                }

                if (prob > 1)
                {
                    prob = 1;
                }

                size_t first    = 0u;
                size_t last     = this->distribution_vec.size();

                if (first == last)
                {
                    std::abort();
                }

                if (first + 1 == last)
                {
                    return first;
                }

                if (prob < this->distribution_vec.front())
                {
                    std::abort();
                }

                if (prob >= this->distribution_vec.back())
                {
                    return last - 1;
                }

                return this->internal_lower_bound_prob_find(prob, first, last);
            }
    };

    class ExpBlockedSearchOptimizerEngine: public virtual MatrixOptimizerEngineInterface
    {
        private:

            using randomizer_t = decltype(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{std::declval<uint32_t>()}));

            std::shared_ptr<MatrixOptimizerEngineInterface> base;

            double exp_base;
            size_t iteration_sz;
            size_t x0;

            static inline constexpr double EXP_BASE_MIN         = 1.1;
            static inline constexpr double EXP_BASE_MAX         = 10;
            static inline constexpr double EXP_BASE_DEFAULT     = 2;

            static inline constexpr double RANGE_EXP_DECAY_RATE = 1.2;

            static inline constexpr size_t X0_DEFAULT           = size_t{1} << 4;

        public:

            ExpBlockedSearchOptimizerEngine(std::shared_ptr<MatrixOptimizerEngineInterface> base_arg,
                                            double exp_base_arg,
                                            size_t iteration_sz_arg,
                                            size_t x0_arg)
            {
                if (base_arg == nullptr)
                {
                    throw std::invalid_argument("bad base argument, null");
                }

                if (std::isnan(exp_base_arg))
                {
                    exp_base_arg    = EXP_BASE_MIN;
                }

                if (x0_arg == 0u)
                {
                    throw std::invalid_argument("bad x0, 0");
                }

                exp_base_arg        = std::clamp(exp_base_arg, EXP_BASE_MIN, EXP_BASE_MAX);

                this->base          = std::move(base_arg);
                this->exp_base      = exp_base_arg;
                this->iteration_sz  = iteration_sz_arg;
                this->x0            = x0_arg;
            }

            ExpBlockedSearchOptimizerEngine(std::shared_ptr<MatrixOptimizerEngineInterface> base_arg,
                                            size_t iteration_sz_arg): ExpBlockedSearchOptimizerEngine(std::move(base_arg),
                                                                                                      EXP_BASE_DEFAULT,
                                                                                                      iteration_sz_arg,
                                                                                                      X0_DEFAULT){}

            auto optimize(the_matrix::MatrixInterface& matrix,
                          matrix_evaluator::MatrixEvaluatorInterface& matrix_evaluator,
                          common_exception::CancellationTokenInterface& cancellation_token) -> std::shared_ptr<the_matrix::MatrixInterface>
            {
                std::shared_ptr<the_matrix::MatrixInterface> current = matrix.clone();

                for (size_t i = 0u; i < this->iteration_sz; ++i)
                {
                    current = this->optimize_one(*current, matrix_evaluator, cancellation_token);
                }

                return current;
            }

        private:

            auto optimize_one(the_matrix::MatrixInterface& matrix,
                              matrix_evaluator::MatrixEvaluatorInterface& matrix_evaluator,
                              common_exception::CancellationTokenInterface& cancellation_token) -> std::shared_ptr<the_matrix::MatrixInterface>
            {
                size_t matrix_sz            = matrix.get_coefficient_vector().size();

                size_t ceil_matrix_sz       = stdx::mul_ceil(matrix_sz, this->x0);
                size_t unscaled_matrix_sz   = ceil_matrix_sz / this->x0;

                double logx_val             = std::log(std::max(size_t{1}, unscaled_matrix_sz));
                double logb_val             = std::log(this->exp_base);

                double tentative_exp_range  = logx_val / logb_val + 2;
                size_t round_exp_range      = static_cast<size_t>(tentative_exp_range);
                size_t fixed_exp_range      = std::max(size_t{1}, round_exp_range);

                size_t range_idx            = ExponentialDistributionRangeRandomizer(fixed_exp_range, RANGE_EXP_DECAY_RATE).randomize();

                size_t operable_sz          = this->x0 * std::pow(this->exp_base, range_idx);
                size_t actual_operable_sz   = std::min(operable_sz, matrix_sz);

                return BlockedSearchOptimizerEngine(this->base, this->get_activation_vector(actual_operable_sz, matrix_sz)).optimize(matrix, matrix_evaluator, cancellation_token);
            }
        
            auto get_activation_vector(size_t active_sz, size_t total_sz) -> std::vector<bool>
            {
                std::vector<bool> rs(total_sz, false);
                size_t actual_active_sz = std::min(active_sz, total_sz);
                std::fill(rs.begin(), std::next(rs.begin(), actual_active_sz), true);

                return rs;
            }
    };
}

#endif