#ifndef __BRANCH_OPTIMIZER_H__
#define __BRANCH_OPTIMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include "conventional_randomizer.h"

namespace branch_optimizer
{
    using branch_float_t = long double;

    class BranchPredictionResultInterface
    {
        public:

            virtual ~BranchPredictionResultInterface() = default;

            virtual auto get_enumeration() -> size_t = 0;
            virtual void feedback(branch_float_t score) = 0;
    };

    class BranchPredictorInterface
    {
        public:

            virtual ~BranchPredictorInterface() noexcept = default;

            virtual auto next() -> std::unique_ptr<BranchPredictionResultInterface> = 0;
            virtual auto size() -> size_t = 0;
    };

    class UniformRandomizer
    {
        private:

            conventional_randomizer::RandomizerObject base_randomizer;
        
        public:

            auto randomize() -> branch_float_t
            {
                const size_t DISCRETIZATION_SZ = 1'000'000'000'000'000ULL;

                return this->base_randomizer.template randomize_fixed_point_float<branch_float_t>(0, 1, DISCRETIZATION_SZ);
            }
    };

    class NormalScorer
    {
        public:

            constexpr auto operator()(branch_float_t total_score, size_t total_count) const noexcept -> branch_float_t
            {
                return static_cast<branch_float_t>(total_score) / total_count;
            }
    };

    class SquareScorer
    {
        public:

            constexpr auto operator()(branch_float_t total_score, size_t total_count) const noexcept -> branch_float_t
            {
                return std::pow(static_cast<branch_float_t>(total_score) / total_count, 2);
            }
    };

    class QuadScorer
    {
        public:

            constexpr auto operator()(branch_float_t total_score, size_t total_count) const noexcept -> branch_float_t
            {
                return std::pow(static_cast<branch_float_t>(total_score) / total_count, 4);
            }
    };

    static inline constexpr branch_float_t MIN_SCORE_VALUE  = 0;
    static inline constexpr branch_float_t MAX_SCORE_VALUE  = 1;

    template <class FloatType>
    class FloatRangeClamper
    {
        private:

            FloatType min_value;
            FloatType max_value;
        
        public:

            static_assert(std::is_floating_point_v<FloatType>);

            FloatRangeClamper(FloatType min_value,
                              FloatType max_value): min_value(min_value),
                                                    max_value(max_value){}

            auto normalize(FloatType x) -> FloatType
            {
                if (std::isnan(x))
                {
                    x = this->min_value;
                }

                return std::clamp(x, this->min_value, this->max_value);
            }
    };

    template <class StatelessScorer = NormalScorer>
    class BranchPredictor: public virtual BranchPredictorInterface
    {
        private:

            struct StatisticalBucket
            {
                branch_float_t total_score;
                size_t total_count;
            };

            struct Resource
            {
                std::vector<StatisticalBucket> good_statistic_table;
                std::vector<branch_float_t> distribution_table;

                size_t enumeration_range;
                size_t evaluation_count;
                size_t reevaluation_window;
            };

            std::shared_ptr<Resource> resource;

            UniformRandomizer randomizer;
            conventional_randomizer::RandomizerObject uint_randomizer;

            static inline constexpr size_t RANDOMIZATION_CHANCE     = 8u;
            static inline constexpr size_t MAX_ENUMERATION_RANGE    = size_t{1} << 16;
            static inline constexpr size_t MAX_REEVALUATION_WINDOW  = size_t{1} << 20;

        public:

            BranchPredictor(size_t enumeration_range,
                            size_t reevaluation_window): randomizer(),
                                                         uint_randomizer()
            {
                if (std::clamp(enumeration_range, size_t{1}, MAX_ENUMERATION_RANGE) != enumeration_range)
                {
                    throw std::invalid_argument("bad enumeration range, out of bound");
                }

                this->resource              = std::make_shared<Resource>(Resource
                {
                    .good_statistic_table   = std::vector<StatisticalBucket>(enumeration_range, StatisticalBucket{.total_score = static_cast<branch_float_t>(0), .total_count = 0u}),
                    .distribution_table     = this->make_initial_distribution_table_for_size_of(enumeration_range),
                    
                    .enumeration_range      = enumeration_range,
                    .evaluation_count       = size_t{0u},
                    .reevaluation_window    = std::clamp(reevaluation_window, size_t{1}, MAX_REEVALUATION_WINDOW)
                });
            }

            BranchPredictor(const BranchPredictor&) = delete;
            BranchPredictor& operator =(const BranchPredictor&) = delete;

            BranchPredictor(BranchPredictor&&) = delete;
            BranchPredictor& operator=(BranchPredictor&&) = delete;

            auto next() -> std::unique_ptr<BranchPredictionResultInterface>
            {
                size_t test_value = this->uint_randomizer.randomize_uint(0u, RANDOMIZATION_CHANCE);

                if (test_value == 0u)
                {
                    return this->random_next();
                }
                else
                {
                    return this->predict_next();
                }
            }

            auto size() -> size_t
            {
                return this->resource->enumeration_range;
            }

        private:

            auto make_initial_distribution_table_for_size_of(size_t sz) -> std::vector<branch_float_t>
            {
                if (sz == 0u)
                {
                    return {};
                }

                std::vector<branch_float_t> result  = {};
                branch_float_t first                = 0;
                branch_float_t inc_val              = branch_float_t{1} / sz;

                for (size_t i = 0u; i < sz; ++i)
                {
                    result.push_back(first);
                    first += inc_val;
                }

                return result;
            }

            auto random_next() -> std::unique_ptr<BranchPredictionResultInterface>
            {
                size_t result = this->uint_randomizer.randomize_uint(0u, this->resource->distribution_table.size());

                return this->make_branch_prediction_result(result);
            }

            auto predict_next() -> std::unique_ptr<BranchPredictionResultInterface>
            {
                branch_float_t probability = this->randomizer.randomize();
                size_t result = this->binary_search_enumeration(probability);

                return this->make_branch_prediction_result(result);
            }

            class BranchPredictionResult: public virtual BranchPredictionResultInterface
            {
                private:

                    std::shared_ptr<Resource> resource;
                    size_t enumeration_idx;
                    bool was_feedback_received;

                public:

                    BranchPredictionResult(std::shared_ptr<Resource> resource,
                                           size_t enumeration_idx,
                                           bool was_feedback_received) noexcept: resource(std::move(resource)),
                                                                                 enumeration_idx(enumeration_idx),
                                                                                 was_feedback_received(was_feedback_received){}

                    auto get_enumeration() -> size_t
                    {
                        return this->enumeration_idx;
                    }

                    void feedback(branch_float_t x)
                    {
                        if (std::exchange(this->was_feedback_received, true))
                        {
                            return;
                        }

                        if (this->resource->evaluation_count == this->resource->reevaluation_window)
                        {
                            this->reevaluate_distribution_table();
                            this->defaultize_statistic_table();

                            this->resource->evaluation_count = 0u;
                        }

                        this->resource->good_statistic_table[this->enumeration_idx].total_score     += FloatRangeClamper<branch_float_t>(MIN_SCORE_VALUE, MAX_SCORE_VALUE).normalize(x);
                        this->resource->good_statistic_table[this->enumeration_idx].total_count     += 1;

                        this->resource->evaluation_count                                            += 1;
                    }

                private:

                    auto get_score(branch_float_t total_score, size_t total_count) -> branch_float_t
                    {
                        return StatelessScorer{}(total_score, total_count);
                    }

                    void defaultize_statistic_table()
                    {
                        std::fill(this->resource->good_statistic_table.begin(),
                                  this->resource->good_statistic_table.end(),
                                  StatisticalBucket{.total_score = static_cast<branch_float_t>(0), .total_count = 0u});
                    }

                    auto get_percentage_table() -> std::vector<branch_float_t>
                    {
                        std::optional<branch_float_t> good_normalization_value = std::nullopt;

                        for (const StatisticalBucket& bucket: this->resource->good_statistic_table)
                        {
                            if (bucket.total_count == 0u)
                            {
                                continue;
                            }

                            if (!good_normalization_value.has_value())
                            {
                                good_normalization_value = this->get_score(bucket.total_score, bucket.total_count);
                                continue;
                            }

                            good_normalization_value.value() += this->get_score(bucket.total_score, bucket.total_count);
                        }

                        if (!good_normalization_value.has_value())
                        {
                            if (this->resource->good_statistic_table.empty())
                            {
                                return {};
                            }

                            return std::vector<branch_float_t>(this->resource->good_statistic_table.size(), static_cast<branch_float_t>(1) / this->resource->good_statistic_table.size());
                        }

                        std::vector<branch_float_t> result{};

                        for (const StatisticalBucket& bucket: this->resource->good_statistic_table)
                        {
                            if (bucket.total_count == 0u)
                            {
                                result.push_back(0);
                                continue;
                            }

                            branch_float_t good_perc    = this->get_score(bucket.total_score, bucket.total_count);
                            branch_float_t perc         = good_perc / good_normalization_value.value();

                            result.push_back(perc);
                        }

                        return result;
                    }

                    void reevaluate_distribution_table()
                    {
                        std::vector<branch_float_t> percentage_table = this->get_percentage_table();
                        branch_float_t first = 0;
                        std::vector<branch_float_t> nxt_distribution_table(percentage_table.size());

                        for (size_t i = 0u; i < percentage_table.size(); ++i)
                        {
                            nxt_distribution_table[i] = first;
                            first += percentage_table[i];
                        }

                        //let's do another validation of increasing sequence

                        try
                        {
                            for (size_t i = 0u; i < nxt_distribution_table.size(); ++i)
                            {
                                if (std::isnan(nxt_distribution_table[i]))
                                {
                                    throw std::runtime_error("unexpected result");
                                }

                                if (i != 0u)
                                {
                                    if (nxt_distribution_table[i] < nxt_distribution_table[i - 1])
                                    {
                                        throw std::runtime_error("unexpected result");
                                    }
                                }
                            }

                            this->resource->distribution_table = std::move(nxt_distribution_table);
                        }
                        catch (...)
                        {
                            return;
                        }
                    }
            };

            auto make_branch_prediction_result(size_t enumeration_idx) -> std::unique_ptr<BranchPredictionResultInterface>
            {
                return std::make_unique<BranchPredictionResult>(this->resource, enumeration_idx, false);
            }

            //this is complicated
            //assume that [front(), ..., back()]
            //and front() <= c < back()

            //assume size is 2 => fine
            //assume size is 3 -> 3 / 2 = 1 => 2 lhs, 2 rhs => fine
            //assume size is 4 -> 4 / 2 = 2 => 3 lhs, 2 rhs => fine
            //assume size is 5 -> 5 / 2 = 2 => 3 lhs, 3 rhs => fine
            //assume size is 6, 7, 8 ..., then lhs = sz / 2 + 1 which should points to the previous sizes => fine

            //by using induction ...

            auto internal_lower_bound_prob_find(branch_float_t prob, size_t first, size_t last) -> size_t
            {
                if (first + 2 == last)
                {
                    return first;
                }

                size_t range_sz     = last - first;
                size_t mid_sz       = range_sz / 2;
                size_t mid_point    = first + mid_sz;

                branch_float_t cand = this->resource->distribution_table[mid_point];

                if (cand > prob)
                {
                    return this->internal_lower_bound_prob_find(prob, first, mid_point + 1);
                }

                return this->internal_lower_bound_prob_find(prob, mid_point, last);
            }

            auto binary_search_enumeration(branch_float_t prob) -> size_t
            {
                if (std::isnan(prob))
                {
                    // throw std::invalid_argument("bad probability, not a number");
                    std::abort();
                }

                if (prob < 0)
                {
                    // throw std::invalid_argument("bad probability, < 0");
                    std::abort();
                }

                if (prob > 1)
                {
                    // throw std::invalid_argument("bad probability, > 1");
                    std::abort();
                }

                size_t first    = 0u;
                size_t last     = this->resource->distribution_table.size();

                if (first == last)
                {
                    // throw std::runtime_error("bad state, internal corruption");
                    std::abort();
                }

                if (first + 1 == last)
                {
                    return first;
                }

                if (prob < this->resource->distribution_table.front())
                {
                    std::abort();
                }

                if (prob >= this->resource->distribution_table.back())
                {
                    return last - 1;
                }

                return this->internal_lower_bound_prob_find(prob, first, last);
            }
    };

    template <class StatelessScorer = NormalScorer>
    class StubbornBranchPredictor: public virtual BranchPredictorInterface
    {
        private:

            struct StatisticalBucket
            {
                branch_float_t total_score;
                size_t total_count;
            };

            struct Resource
            {
                std::vector<StatisticalBucket> good_statistic_table;
                std::vector<branch_float_t> distribution_table;

                size_t enumeration_range;
                size_t evaluation_count;
                size_t reevaluation_window;
            };

            std::shared_ptr<Resource> resource;

            UniformRandomizer randomizer;
            conventional_randomizer::RandomizerObject uint_randomizer;

            static inline constexpr size_t RANDOMIZATION_CHANCE     = 8u;
            static inline constexpr size_t MAX_ENUMERATION_RANGE    = size_t{1} << 16;
            static inline constexpr size_t MAX_REEVALUATION_WINDOW  = size_t{1} << 20;
            static inline constexpr size_t RESET_THRESHOLD          = size_t{1} << 40;

        public:

            StubbornBranchPredictor(size_t enumeration_range,
                                    size_t reevaluation_window): randomizer(),
                                                                 uint_randomizer()
            {
                if (std::clamp(enumeration_range, size_t{1}, MAX_ENUMERATION_RANGE) != enumeration_range)
                {
                    throw std::invalid_argument("bad enumeration range, out of bound");
                }

                this->resource              = std::make_shared<Resource>(Resource
                {
                    .good_statistic_table   = std::vector<StatisticalBucket>(enumeration_range, StatisticalBucket{.total_score = static_cast<branch_float_t>(0), .total_count = 0u}),
                    .distribution_table     = this->make_initial_distribution_table_for_size_of(enumeration_range),
                    
                    .enumeration_range      = enumeration_range,
                    .evaluation_count       = size_t{0u},
                    .reevaluation_window    = std::clamp(reevaluation_window, size_t{1}, MAX_REEVALUATION_WINDOW)
                });
            }

            StubbornBranchPredictor(const StubbornBranchPredictor&) = delete;
            StubbornBranchPredictor& operator =(const StubbornBranchPredictor&) = delete;

            StubbornBranchPredictor(StubbornBranchPredictor&&) = delete;
            StubbornBranchPredictor& operator=(StubbornBranchPredictor&&) = delete;

            auto next() -> std::unique_ptr<BranchPredictionResultInterface>
            {
                size_t test_value = this->uint_randomizer.randomize_uint(0u, RANDOMIZATION_CHANCE);

                if (test_value == 0u)
                {
                    return this->random_next();
                }
                else
                {
                    return this->predict_next();
                }
            }

            auto size() -> size_t
            {
                return this->resource->enumeration_range;
            }

        private:

            auto make_initial_distribution_table_for_size_of(size_t sz) -> std::vector<branch_float_t>
            {
                if (sz == 0u)
                {
                    return {};
                }

                std::vector<branch_float_t> result  = {};
                branch_float_t first                = 0;
                branch_float_t inc_val              = branch_float_t{1} / sz;

                for (size_t i = 0u; i < sz; ++i)
                {
                    result.push_back(first);
                    first += inc_val;
                }

                return result;
            }

            auto random_next() -> std::unique_ptr<BranchPredictionResultInterface>
            {
                size_t result = this->uint_randomizer.randomize_uint(0u, this->resource->distribution_table.size());

                return this->make_branch_prediction_result(result);
            }

            auto predict_next() -> std::unique_ptr<BranchPredictionResultInterface>
            {
                branch_float_t probability = this->randomizer.randomize();
                size_t result = this->binary_search_enumeration(probability);

                return this->make_branch_prediction_result(result);
            }

            class BranchPredictionResult: public virtual BranchPredictionResultInterface
            {
                private:

                    std::shared_ptr<Resource> resource;
                    size_t enumeration_idx;
                    bool was_feedback_received;

                public:

                    BranchPredictionResult(std::shared_ptr<Resource> resource,
                                           size_t enumeration_idx,
                                           bool was_feedback_received) noexcept: resource(std::move(resource)),
                                                                                 enumeration_idx(enumeration_idx),
                                                                                 was_feedback_received(was_feedback_received){}

                    auto get_enumeration() -> size_t
                    {
                        return this->enumeration_idx;
                    }

                    void feedback(branch_float_t x)
                    {
                        if (std::exchange(this->was_feedback_received, true))
                        {
                            return;
                        }

                        if (this->resource->evaluation_count == this->resource->reevaluation_window)
                        {
                            this->reevaluate_distribution_table();

                            if (this->is_statistic_table_full())
                            {
                                this->defaultize_statistic_table();
                            }

                            this->resource->evaluation_count = 0u;
                        }

                        this->resource->good_statistic_table[this->enumeration_idx].total_score     += FloatRangeClamper<branch_float_t>(MIN_SCORE_VALUE, MAX_SCORE_VALUE).normalize(x);
                        this->resource->good_statistic_table[this->enumeration_idx].total_count     += 1;

                        this->resource->evaluation_count                                            += 1;
                    }

                private:

                    auto is_statistic_table_full() -> bool
                    {
                        for (const StatisticalBucket& bucket: this->resource->good_statistic_table)
                        {
                            if (bucket.total_count >= RESET_THRESHOLD)
                            {
                                return true;
                            }
                        }

                        return false;
                    }

                    auto get_score(branch_float_t total_score, size_t total_count) -> branch_float_t
                    {
                        return StatelessScorer{}(total_score, total_count);
                    }

                    void defaultize_statistic_table()
                    {
                        std::fill(this->resource->good_statistic_table.begin(),
                                  this->resource->good_statistic_table.end(),
                                  StatisticalBucket{.total_score = static_cast<branch_float_t>(0), .total_count = 0u});
                    }

                    auto get_percentage_table() -> std::vector<branch_float_t>
                    {
                        std::optional<branch_float_t> good_normalization_value = std::nullopt;

                        for (const StatisticalBucket& bucket: this->resource->good_statistic_table)
                        {
                            if (bucket.total_count == 0u)
                            {
                                continue;
                            }

                            if (!good_normalization_value.has_value())
                            {
                                good_normalization_value = this->get_score(bucket.total_score, bucket.total_count);
                                continue;
                            }

                            good_normalization_value.value() += this->get_score(bucket.total_score, bucket.total_count);
                        }

                        if (!good_normalization_value.has_value())
                        {
                            if (this->resource->good_statistic_table.empty())
                            {
                                return {};
                            }

                            return std::vector<branch_float_t>(this->resource->good_statistic_table.size(), static_cast<branch_float_t>(1) / this->resource->good_statistic_table.size());
                        }

                        std::vector<branch_float_t> result{};

                        for (const StatisticalBucket& bucket: this->resource->good_statistic_table)
                        {
                            if (bucket.total_count == 0u)
                            {
                                result.push_back(0);
                                continue;
                            }

                            branch_float_t good_perc    = this->get_score(bucket.total_score, bucket.total_count);
                            branch_float_t perc         = good_perc / good_normalization_value.value();

                            result.push_back(perc);
                        }

                        return result;
                    }

                    void reevaluate_distribution_table()
                    {
                        std::vector<branch_float_t> percentage_table = this->get_percentage_table();
                        branch_float_t first = 0;
                        std::vector<branch_float_t> nxt_distribution_table(percentage_table.size());

                        for (size_t i = 0u; i < percentage_table.size(); ++i)
                        {
                            nxt_distribution_table[i] = first;
                            first += percentage_table[i];
                        }

                        //let's do another validation of increasing sequence

                        try
                        {
                            for (size_t i = 0u; i < nxt_distribution_table.size(); ++i)
                            {
                                if (std::isnan(nxt_distribution_table[i]))
                                {
                                    throw std::runtime_error("unexpected result");
                                }

                                if (i != 0u)
                                {
                                    if (nxt_distribution_table[i] < nxt_distribution_table[i - 1])
                                    {
                                        throw std::runtime_error("unexpected result");
                                    }
                                }
                            }

                            this->resource->distribution_table = std::move(nxt_distribution_table);
                        }
                        catch (...)
                        {
                            return;
                        }
                    }
            };

            auto make_branch_prediction_result(size_t enumeration_idx) -> std::unique_ptr<BranchPredictionResultInterface>
            {
                return std::make_unique<BranchPredictionResult>(this->resource, enumeration_idx, false);
            }

            //this is complicated
            //assume that [front(), ..., back()]
            //and front() <= c < back()

            //assume size is 2 => fine
            //assume size is 3 -> 3 / 2 = 1 => 2 lhs, 2 rhs => fine
            //assume size is 4 -> 4 / 2 = 2 => 3 lhs, 2 rhs => fine
            //assume size is 5 -> 5 / 2 = 2 => 3 lhs, 3 rhs => fine
            //assume size is 6, 7, 8 ..., then lhs = sz / 2 + 1 which should points to the previous sizes => fine

            //by using induction ...

            auto internal_lower_bound_prob_find(branch_float_t prob, size_t first, size_t last) -> size_t
            {
                if (first + 2 == last)
                {
                    return first;
                }

                size_t range_sz     = last - first;
                size_t mid_sz       = range_sz / 2;
                size_t mid_point    = first + mid_sz;

                branch_float_t cand = this->resource->distribution_table[mid_point];

                if (cand > prob)
                {
                    return this->internal_lower_bound_prob_find(prob, first, mid_point + 1);
                }

                return this->internal_lower_bound_prob_find(prob, mid_point, last);
            }

            auto binary_search_enumeration(branch_float_t prob) -> size_t
            {
                if (std::isnan(prob))
                {
                    // throw std::invalid_argument("bad probability, not a number");
                    std::abort();
                }

                if (prob < 0)
                {
                    // throw std::invalid_argument("bad probability, < 0");
                    std::abort();
                }

                if (prob > 1)
                {
                    // throw std::invalid_argument("bad probability, > 1");
                    std::abort();
                }

                size_t first    = 0u;
                size_t last     = this->resource->distribution_table.size();

                if (first == last)
                {
                    // throw std::runtime_error("bad state, internal corruption");
                    std::abort();
                }

                if (first + 1 == last)
                {
                    return first;
                }

                if (prob < this->resource->distribution_table.front())
                {
                    std::abort();
                }

                if (prob >= this->resource->distribution_table.back())
                {
                    return last - 1;
                }

                return this->internal_lower_bound_prob_find(prob, first, last);
            }
    };

    class ProbabilisticBranchPredictor: public virtual BranchPredictorInterface
    {
        private:

            std::unique_ptr<BranchPredictorInterface> lhs_scorer;
            std::unique_ptr<BranchPredictorInterface> rhs_scorer;
            conventional_randomizer::RandomizerObject uint_randomizer;
            size_t lhs_chance;
            size_t lhs_threshold;

        public:

            ProbabilisticBranchPredictor(std::unique_ptr<BranchPredictorInterface> lhs_scorer,
                                         std::unique_ptr<BranchPredictorInterface> rhs_scorer,
                                         conventional_randomizer::RandomizerObject uint_randomizer,
                                         size_t lhs_chance,
                                         size_t lhs_threshold) noexcept: lhs_scorer(std::move(lhs_scorer)),
                                                                         rhs_scorer(std::move(rhs_scorer)),
                                                                         uint_randomizer(std::move(uint_randomizer)),
                                                                         lhs_chance(lhs_chance),
                                                                         lhs_threshold(lhs_threshold){}

            auto next() -> std::unique_ptr<BranchPredictionResultInterface>
            {
                size_t dice = this->uint_randomizer.randomize_uint(0u, this->lhs_chance);

                if (dice >= this->lhs_threshold)
                {
                    return this->lhs_scorer->next();
                }
                else
                {
                    return this->rhs_scorer->next();
                }
            }

            auto size() -> size_t
            {
                return this->lhs_scorer->size();
            }
    };

    class BranchPredictorFactory
    {
        public:

            using branch_predictor_t = uint8_t;

            static inline constexpr branch_predictor_t NORMAL_BRANCH_PREDICTOR_ID                   = 0u;
            static inline constexpr branch_predictor_t SQUARE_BRANCH_PREDICTOR_ID                   = 1u;
            static inline constexpr branch_predictor_t QUAD_BRANCH_PREDICTOR_ID                     = 2u;
            static inline constexpr branch_predictor_t NORMAL_SQUARE_BRANCH_PREDICTOR_ID            = 3u;

            static inline constexpr branch_predictor_t NORMAL_ADAPTIVE_BRANCH_PREDICTOR_ID          = 4u;
            static inline constexpr branch_predictor_t SQUARE_ADAPTIVE_BRANCH_PREDICTOR_ID          = 5u;
            static inline constexpr branch_predictor_t QUAD_ADAPTIVE_BRANCH_PREDICTOR_ID            = 6u;
            static inline constexpr branch_predictor_t NORMAL_SQUARE_ADAPTIVE_BRANCH_PREDICTOR_ID   = 7u;
            static inline constexpr branch_predictor_t NORMAL_SQUARE_2_BRANCH_PREDICTOR_ID          = 8u;

            static auto get_normal_branch_predictor(size_t enumeration_range,
                                                    std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                const size_t FAIR_SAMPLE_SZ             = size_t{1} << 3;
                const size_t MIN_REEVALUATION_RANGE     = std::max(static_cast<size_t>(enumeration_range >> 8), FAIR_SAMPLE_SZ);

                if (!reevaluation_range.has_value())
                {
                    reevaluation_range = enumeration_range;
                }

                reevaluation_range.value()              = std::max(reevaluation_range.value(), MIN_REEVALUATION_RANGE);

                return std::make_unique<StubbornBranchPredictor<NormalScorer>>(enumeration_range, reevaluation_range.value());
            }

            static auto get_square_branch_predictor(size_t enumeration_range,
                                                    std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                const size_t FAIR_SAMPLE_SZ             = size_t{1} << 3;
                const size_t MIN_REEVALUATION_RANGE     = std::max(static_cast<size_t>(enumeration_range >> 8), FAIR_SAMPLE_SZ);

                if (!reevaluation_range.has_value())
                {
                    reevaluation_range = enumeration_range;
                }

                reevaluation_range.value()              = std::max(reevaluation_range.value(), MIN_REEVALUATION_RANGE);

                return std::make_unique<StubbornBranchPredictor<SquareScorer>>(enumeration_range, reevaluation_range.value());
            }

            static auto get_quad_branch_predictor(size_t enumeration_range,
                                                  std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                const size_t FAIR_SAMPLE_SZ             = size_t{1} << 3;
                const size_t MIN_REEVALUATION_RANGE     = std::max(static_cast<size_t>(enumeration_range >> 8), FAIR_SAMPLE_SZ);

                if (!reevaluation_range.has_value())
                {
                    reevaluation_range = enumeration_range;
                }

                reevaluation_range.value()              = std::max(reevaluation_range.value(), MIN_REEVALUATION_RANGE);

                return std::make_unique<StubbornBranchPredictor<QuadScorer>>(enumeration_range, reevaluation_range.value());
            }

            static auto get_normal_square_branch_predictor(size_t enumeration_range,
                                                           std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                return std::make_unique<ProbabilisticBranchPredictor>(get_normal_branch_predictor(enumeration_range, reevaluation_range),
                                                                      get_square_branch_predictor(enumeration_range, reevaluation_range),
                                                                      conventional_randomizer::RandomizerObject(),
                                                                      4u,
                                                                      3u);
            }

            static auto get_normal_adaptive_branch_predictor(size_t enumeration_range,
                                                             std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                const size_t FAIR_SAMPLE_SZ             = size_t{1} << 4;
                const size_t MIN_REEVALUATION_RANGE     = std::max(static_cast<size_t>(enumeration_range >> 2), FAIR_SAMPLE_SZ);
                const size_t STABLE_MULTIPLIER          = size_t{1} << 3;

                if (!reevaluation_range.has_value())
                {
                    reevaluation_range = enumeration_range * STABLE_MULTIPLIER;
                }

                reevaluation_range.value()              = std::max(reevaluation_range.value(), MIN_REEVALUATION_RANGE);

                return std::make_unique<BranchPredictor<NormalScorer>>(enumeration_range, reevaluation_range.value());
            }

            static auto get_square_adaptive_branch_predictor(size_t enumeration_range,
                                                             std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                const size_t FAIR_SAMPLE_SZ             = size_t{1} << 4;
                const size_t MIN_REEVALUATION_RANGE     = std::max(static_cast<size_t>(enumeration_range >> 2), FAIR_SAMPLE_SZ);
                const size_t STABLE_MULTIPLIER          = size_t{1} << 3;

                if (!reevaluation_range.has_value())
                {
                    reevaluation_range = enumeration_range * STABLE_MULTIPLIER;
                }

                reevaluation_range.value()              = std::max(reevaluation_range.value(), MIN_REEVALUATION_RANGE);

                return std::make_unique<BranchPredictor<SquareScorer>>(enumeration_range, reevaluation_range.value());
            }

            static auto get_quad_adaptive_branch_predictor(size_t enumeration_range,
                                                           std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                const size_t FAIR_SAMPLE_SZ             = size_t{1} << 4;
                const size_t MIN_REEVALUATION_RANGE     = std::max(static_cast<size_t>(enumeration_range >> 2), FAIR_SAMPLE_SZ);
                const size_t STABLE_MULTIPLIER          = size_t{1} << 3;

                if (!reevaluation_range.has_value())
                {
                    reevaluation_range = enumeration_range * STABLE_MULTIPLIER;
                }

                reevaluation_range.value()              = std::max(reevaluation_range.value(), MIN_REEVALUATION_RANGE);

                return std::make_unique<BranchPredictor<QuadScorer>>(enumeration_range, reevaluation_range.value());
            }

            static auto get_normal_square_adaptive_branch_predictor(size_t enumeration_range,
                                                                    std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                return std::make_unique<ProbabilisticBranchPredictor>(get_normal_adaptive_branch_predictor(enumeration_range, reevaluation_range),
                                                                      get_square_adaptive_branch_predictor(enumeration_range, reevaluation_range),
                                                                      conventional_randomizer::RandomizerObject(),
                                                                      4u,
                                                                      3u);
            }

            static auto get_normal_square_2_branch_predictor(size_t enumeration_range,
                                                             std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                std::optional<size_t> fast_track_sz;

                if (reevaluation_range.has_value())
                {
                    fast_track_sz = reevaluation_range.value() >> 8;
                }
                else
                {
                    fast_track_sz = std::nullopt;
                }

                return std::make_unique<ProbabilisticBranchPredictor>(get_normal_square_branch_predictor(enumeration_range, reevaluation_range),
                                                                      get_normal_square_adaptive_branch_predictor(enumeration_range, fast_track_sz),
                                                                      conventional_randomizer::RandomizerObject(),
                                                                      2u,
                                                                      1u);
            }

            static auto get_branch_predictor(branch_predictor_t predictor_kind,
                                             size_t enumeration_range,
                                             std::optional<size_t> reevaluation_range = std::nullopt) -> std::unique_ptr<BranchPredictorInterface>
            {
                switch (predictor_kind)
                {
                    case NORMAL_BRANCH_PREDICTOR_ID:
                    {
                        return get_normal_branch_predictor(enumeration_range, reevaluation_range);
                    }
                    case SQUARE_BRANCH_PREDICTOR_ID:
                    {
                        return get_square_branch_predictor(enumeration_range, reevaluation_range);
                    }
                    case QUAD_BRANCH_PREDICTOR_ID:
                    {
                        return get_quad_branch_predictor(enumeration_range, reevaluation_range);
                    }
                    case NORMAL_SQUARE_BRANCH_PREDICTOR_ID:
                    {
                        return get_normal_square_branch_predictor(enumeration_range, reevaluation_range);
                    }
                    case NORMAL_ADAPTIVE_BRANCH_PREDICTOR_ID:
                    {
                        return get_normal_adaptive_branch_predictor(enumeration_range, reevaluation_range);
                    }
                    case SQUARE_ADAPTIVE_BRANCH_PREDICTOR_ID:
                    {
                        return get_square_adaptive_branch_predictor(enumeration_range, reevaluation_range);
                    }
                    case QUAD_ADAPTIVE_BRANCH_PREDICTOR_ID:
                    {
                        return get_quad_adaptive_branch_predictor(enumeration_range, reevaluation_range);
                    }
                    case NORMAL_SQUARE_ADAPTIVE_BRANCH_PREDICTOR_ID:
                    {
                        return get_normal_square_adaptive_branch_predictor(enumeration_range, reevaluation_range);
                    }
                    case NORMAL_SQUARE_2_BRANCH_PREDICTOR_ID:
                    {
                        return get_normal_square_2_branch_predictor(enumeration_range, reevaluation_range);
                    }
                    default:
                    {
                        throw std::invalid_argument("bad predictor kind, no predictor matched");
                    }
                }
            }
    };

    class MultipleBranchPredictionResultInterface
    {
        public:

            virtual ~MultipleBranchPredictionResultInterface() = default;
            virtual auto get_enumeration() -> std::vector<size_t> = 0;
            virtual void feedback(branch_float_t score) = 0;
    };

    class MultipleBranchPredictorInterface
    {
        public:

            virtual ~MultipleBranchPredictorInterface() = default;
            virtual auto next() -> std::unique_ptr<MultipleBranchPredictionResultInterface> = 0;
    };

    class HierarchicalBranchPredictor: public virtual MultipleBranchPredictorInterface
    {
        private:

            struct DecisionTree
            {
                std::unique_ptr<BranchPredictorInterface> predictor;
                std::vector<std::unique_ptr<DecisionTree>> child_vec;
            };

            std::shared_ptr<DecisionTree> root;

        public:

            HierarchicalBranchPredictor(const std::vector<std::pair<size_t, BranchPredictorFactory::branch_predictor_t>>& preorder_enum_tree,
                                        std::optional<size_t> reevaluation_window)
            {
                this->root = this->construct_from_preorder_tree(preorder_enum_tree, reevaluation_window);
            }

            auto next() -> std::unique_ptr<MultipleBranchPredictionResultInterface>
            {
                DecisionTree * cur = this->root.get();
                std::vector<std::unique_ptr<BranchPredictionResultInterface>> branch_vec{};

                while (true)
                {
                    if (cur == nullptr)
                    {
                        break;
                    }

                    branch_vec.push_back(cur->predictor->next());
                    size_t branch_idx = branch_vec.back()->get_enumeration();

                    if (branch_idx >= cur->child_vec.size())
                    {
                        std::abort();
                    }

                    cur = cur->child_vec[branch_idx].get();
                }

                return this->make_branch_prediction_result(std::move(branch_vec));
            }

        private:

            auto construct_from_preorder_tree_helper(const std::vector<std::pair<size_t, BranchPredictorFactory::branch_predictor_t>>& preorder_enum_tree_arg,
                                                     std::optional<size_t> reevaluation_window,
                                                     size_t& offset) -> std::unique_ptr<DecisionTree>
            {
                if (offset >= preorder_enum_tree_arg.size())
                {
                    throw std::invalid_argument("bad tree construction, invalid preorder format");
                }

                auto [enum_sz, factory_kind] = preorder_enum_tree_arg[offset++];

                if (enum_sz == 0u)
                {
                    return nullptr;
                }

                if (enum_sz == 1u)
                {
                    throw std::invalid_argument("bad tree construction, suffix size 1");
                }

                std::unique_ptr<DecisionTree> result = std::make_unique<DecisionTree>(DecisionTree
                {
                    .predictor  = BranchPredictorFactory::get_branch_predictor(factory_kind, enum_sz, reevaluation_window),
                    .child_vec  = {}
                });

                for (size_t i = 0u; i < enum_sz; ++i)
                {
                    result->child_vec.push_back(this->construct_from_preorder_tree_helper(preorder_enum_tree_arg, reevaluation_window, offset));
                }

                return result;
            }

            auto construct_from_preorder_tree(const std::vector<std::pair<size_t, BranchPredictorFactory::branch_predictor_t>>& preorder_enum_tree_arg,
                                              std::optional<size_t> reevaluation_window) -> std::unique_ptr<DecisionTree>
            {
                size_t offset = 0u;

                return this->construct_from_preorder_tree_helper(preorder_enum_tree_arg,
                                                                 reevaluation_window,
                                                                 offset);
            }

            class MultipleBranchPredictionResult: public virtual MultipleBranchPredictionResultInterface
            {
                private:

                    std::vector<std::unique_ptr<BranchPredictionResultInterface>> branch_tensor_vec;
                    std::shared_ptr<DecisionTree> root;

                public:

                    MultipleBranchPredictionResult(std::vector<std::unique_ptr<BranchPredictionResultInterface>> branch_tensor_vec,
                                                   std::shared_ptr<DecisionTree> root) noexcept: branch_tensor_vec(std::move(branch_tensor_vec)),
                                                                                                 root(std::move(root)){}

                    auto get_enumeration() -> std::vector<size_t>
                    {
                        DecisionTree * cur              = this->root.get();
                        std::vector<size_t> result_vec  = {};

                        for (size_t i = 0u; i < this->branch_tensor_vec.size(); ++i)
                        {
                            result_vec.push_back(this->branch_tensor_vec[i]->get_enumeration());
                            cur = cur->child_vec[result_vec.back()].get();
                        }

                        return result_vec;
                    }

                    void feedback(branch_float_t x)
                    {
                        for (const auto& e: this->branch_tensor_vec)
                        {
                            e->feedback(x);
                        }
                    }
            };

            auto make_branch_prediction_result(std::vector<std::unique_ptr<BranchPredictionResultInterface>> branch_vec) -> std::unique_ptr<MultipleBranchPredictionResultInterface>
            {
                return std::make_unique<MultipleBranchPredictionResult>(std::move(branch_vec),
                                                                        this->root);
            }
    };

    class ProbabilisticMultipleBranchPredictor: public virtual MultipleBranchPredictorInterface
    {
        private:

            std::unique_ptr<MultipleBranchPredictorInterface> lhs_scorer;
            std::unique_ptr<MultipleBranchPredictorInterface> rhs_scorer;
            conventional_randomizer::RandomizerObject uint_randomizer;
            size_t lhs_chance;
            size_t lhs_threshold;
        
        public:

            ProbabilisticMultipleBranchPredictor(std::unique_ptr<MultipleBranchPredictorInterface> lhs_scorer,
                                                 std::unique_ptr<MultipleBranchPredictorInterface> rhs_scorer,
                                                 conventional_randomizer::RandomizerObject uint_randomizer,
                                                 size_t lhs_chance,
                                                 size_t lhs_threshold) noexcept: lhs_scorer(std::move(lhs_scorer)),
                                                                                 rhs_scorer(std::move(rhs_scorer)),
                                                                                 uint_randomizer(std::move(uint_randomizer)),
                                                                                 lhs_chance(lhs_chance),
                                                                                 lhs_threshold(lhs_threshold){}

            auto next() -> std::unique_ptr<MultipleBranchPredictionResultInterface>
            {
                size_t dice = this->uint_randomizer.randomize_uint(0u, this->lhs_chance);

                if (dice >= this->lhs_threshold)
                {
                    return this->lhs_scorer->next();
                }
                else
                {
                    return this->rhs_scorer->next();
                }
            }
    };

    class HierarchicalBranchPredictorFactory
    {
        public:

            static auto get_traditional_branch_predictor_from_preorder_tree(const std::vector<size_t>& preorder_enumeration_table,
                                                                            std::optional<size_t> reevaluation_window = std::nullopt) -> std::unique_ptr<MultipleBranchPredictorInterface>
            {
                std::vector<std::pair<size_t, BranchPredictorFactory::branch_predictor_t>> branch_enumeration_table{};

                for (size_t enumeration: preorder_enumeration_table)
                {
                    branch_enumeration_table.push_back({enumeration, BranchPredictorFactory::NORMAL_BRANCH_PREDICTOR_ID});
                }

                return std::make_unique<HierarchicalBranchPredictor>(std::move(branch_enumeration_table), reevaluation_window);
            }

            static auto get_aggressive_branch_predictor_from_preorder_tree(const std::vector<size_t>& preorder_enumeration_table,
                                                                           std::optional<size_t> reevaluation_window = std::nullopt) -> std::unique_ptr<MultipleBranchPredictorInterface>
            {
                std::vector<std::pair<size_t, BranchPredictorFactory::branch_predictor_t>> branch_enumeration_table{};

                for (size_t enumeration: preorder_enumeration_table)
                {
                    branch_enumeration_table.push_back({enumeration, BranchPredictorFactory::NORMAL_SQUARE_BRANCH_PREDICTOR_ID});
                }

                return std::make_unique<HierarchicalBranchPredictor>(std::move(branch_enumeration_table), reevaluation_window);
            }

            static auto get_traditional_adaptive_branch_predictor_from_preorder_tree(const std::vector<size_t>& preorder_enumeration_table,
                                                                                     std::optional<size_t> reevaluation_window = std::nullopt) -> std::unique_ptr<MultipleBranchPredictorInterface>
            {
                std::vector<std::pair<size_t, BranchPredictorFactory::branch_predictor_t>> branch_enumeration_table{};

                for (size_t enumeration: preorder_enumeration_table)
                {
                    branch_enumeration_table.push_back({enumeration, BranchPredictorFactory::NORMAL_ADAPTIVE_BRANCH_PREDICTOR_ID});
                }

                return std::make_unique<HierarchicalBranchPredictor>(std::move(branch_enumeration_table), reevaluation_window);
            }

            static auto get_aggressive_adaptive_branch_predictor_from_preorder_tree(const std::vector<size_t>& preorder_enumeration_table,
                                                                                    std::optional<size_t> reevaluation_window = std::nullopt) -> std::unique_ptr<MultipleBranchPredictorInterface>
            {
                std::vector<std::pair<size_t, BranchPredictorFactory::branch_predictor_t>> branch_enumeration_table{};

                for (size_t enumeration: preorder_enumeration_table)
                {
                    branch_enumeration_table.push_back({enumeration, BranchPredictorFactory::NORMAL_SQUARE_ADAPTIVE_BRANCH_PREDICTOR_ID});
                }

                return std::make_unique<HierarchicalBranchPredictor>(std::move(branch_enumeration_table), reevaluation_window);
            }

            static auto get_best_branch_predictor_from_preorder_tree(const std::vector<size_t>& preorder_enumeration_table) -> std::unique_ptr<MultipleBranchPredictorInterface>
            {
                auto lhs    = std::make_unique<ProbabilisticMultipleBranchPredictor>(get_traditional_branch_predictor_from_preorder_tree(preorder_enumeration_table),
                                                                                     get_aggressive_branch_predictor_from_preorder_tree(preorder_enumeration_table),
                                                                                     conventional_randomizer::RandomizerObject(),
                                                                                     2u,
                                                                                     1u);
                
                auto rhs    = std::make_unique<ProbabilisticMultipleBranchPredictor>(get_traditional_adaptive_branch_predictor_from_preorder_tree(preorder_enumeration_table),
                                                                                     get_aggressive_adaptive_branch_predictor_from_preorder_tree(preorder_enumeration_table),
                                                                                     conventional_randomizer::RandomizerObject(),
                                                                                     2u,
                                                                                     1u);

                return std::make_unique<ProbabilisticMultipleBranchPredictor>(std::move(lhs),
                                                                              std::move(rhs),
                                                                              conventional_randomizer::RandomizerObject(),
                                                                              2u,
                                                                              1u);
            }
    };
}

#endif