#ifndef __STOCK_SOLUTION_H__
#define __STOCK_SOLUTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include <utility>
#include <vector>
#include <string>
#include <chrono>
#include "stdx.h"
#include <unordered_map>
#include <unordered_set>
#include <map>
#include "the_matrix_interface.h"
#include "tensor_matrix_operation.h"
#include "matrix_optimizer_interface.h"
#include <numeric>
#include <functional>
#include "matrix_optimizer.h"
#include "the_host_matrix.h"

namespace stock_solution
{
    //let's see if we can make it 1 million bucks/ second this week, stay very tuned
    //are you IN THE MONEY or OUT THE MONEY?
    //we'd attempt to do a 10-100 million extraction, then that's it. On the candlesticks!
    //we only need to be confident once in a while, that's how we do that, baby!

    //we actually rotate all the stocks in the world at once, by using focal and suffix compression (you wont believe how efficient suffix compression is in the semantic space)
    //and we'd want to predict the next second to be on the candlesticks

    //ideally, we'd want to buy immediately and sell to a higher bidder, or cut loss in the next second
    //we'd try something tmr, just for the proof of concept, we'd dry run if it could turn 1$ -> 1M
    //I'm positive at this point that we could literally put everything in the matrix for consensus
    //the problem is still that we'd have to have an exponential input -> output window + entropy scope for the matrix, larger context requires multiple previous transformation means next input
    //this requires two matrices, the action matrix and the mind matrix
    //or we'd just have one very big matrix, because essentially we could logically mask the input, to reduce the ambiguity

    struct TickerData
    {
        std::string ticker_name;
        std::string feature_name;
        double feature_value;
        std::chrono::time_point<std::chrono::system_clock> timestamp;
    };

    class SequenceCompressor
    {
        public:

            auto suffix_lossless_compress(const std::vector<double>& vec) -> std::pair<std::vector<size_t>, std::vector<double>>
            {
                stdx::safe_float_range_access(vec.data(), vec.size());

                std::vector<std::pair<size_t, double>> enumerated_vec   = stdx::enumerate_vector(vec);
                std::vector<double> successor_vec                       = {};
                std::vector<size_t> suffix_vec                          = {};
                auto less_cmp                                           = [](const auto& lhs, const auto& rhs)
                {
                    return lhs.second < rhs.second;
                };

                std::stable_sort(enumerated_vec.begin(), enumerated_vec.end(), less_cmp);

                for (size_t i = 0u; i < enumerated_vec.size(); ++i)
                {
                    if (i == 0u)
                    {
                        successor_vec.push_back(enumerated_vec[i].second);
                    }
                    else
                    {
                        double ratio = enumerated_vec[i].second / enumerated_vec[i - 1].second;

                        if (std::isnan(ratio))
                        {
                            throw std::runtime_error("bad ratio, NaN");
                        }

                        successor_vec.push_back(ratio);
                    }

                    suffix_vec.push_back(enumerated_vec[i].first);
                }

                return std::make_pair(std::move(suffix_vec), std::move(successor_vec));
            }
    };

    class Base10ExponentialRadixer
    {
        private:

            static inline constexpr double EXP_BASE                 = 10;
            static inline constexpr size_t SIGNLESS_ENUMERATION_SZ  = 10u;
            static inline constexpr size_t ENUMERATION_SZ           = 20u;

        public:

            auto enumerate(double value) -> size_t
            {
                if (std::isnan(value))
                {
                    throw std::invalid_argument("bad value, NaN");
                }

                double signless_value;
                size_t offset;

                if (value < 0)
                {
                    signless_value  = -value;
                    offset          = SIGNLESS_ENUMERATION_SZ;
                }
                else
                {
                    signless_value  = value;
                    offset          = 0u;
                }

                for (size_t i = 0u; i < SIGNLESS_ENUMERATION_SZ; ++i)
                {
                    double tentative_value = std::pow(EXP_BASE, i);

                    if (tentative_value > signless_value)
                    {
                        return i + offset;
                    }
                }

                return (SIGNLESS_ENUMERATION_SZ - 1u) + offset;
            }

            auto enumeration_size() -> size_t
            {
                return this->ENUMERATION_SZ;
            }
    };

    struct FeatureAnalyticPoint
    {
        std::string feature_id;
        size_t focal_idx;
        bool bear_or_bull;
        double confident_score;
    };

    struct FeatureAnalyticReport
    {
        std::vector<FeatureAnalyticPoint> analytic_point_vec;
    };

    template <class Reducer>
    class IntervalReducedTree
    {
        private:

            std::vector<std::optional<double>> interval_tree;
            Reducer reducer;
            size_t virtual_sz;
            size_t actual_sz;

        public:

            IntervalReducedTree(const std::vector<double>& managing_interval,
                                const Reducer& reducer = Reducer())
            {
                stdx::safe_float_range_access(managing_interval.data(), managing_interval.size());

                this->actual_sz     = this->get_base_size(managing_interval.size());
                this->interval_tree = this->make_interval_tree_from(managing_interval, reducer, this->actual_sz);
                this->virtual_sz    = managing_interval.size();
                this->reducer       = reducer;
            }

            auto get(const std::pair<size_t, size_t>& interval) -> double
            {
                size_t first        = interval.first;
                size_t last         = interval.first + interval.second;

                if (last > this->virtual_sz)
                {
                    throw std::invalid_argument("bad access, out of range access");
                }

                if (first == last)
                {
                    throw std::invalid_argument("bad interval, 0 interval");
                }

                size_t tree_first   = 0u;
                size_t tree_last    = this->actual_sz;
                size_t key_first    = first;
                size_t key_last     = last;
                size_t idx          = 0u;

                std::optional<double> result = this->get_helper(this->interval_tree,
                                                                tree_first, tree_last,
                                                                key_first, key_last,
                                                                idx,
                                                                this->reducer);

                if (!result.has_value())
                {
                    std::abort();
                }

                return result.value();
            }

        private:

            auto get_base_size(size_t managing_interval_sz) -> size_t
            {
                return size_t{1} << stdx::ulog2(stdx::ceil2(managing_interval_sz));
            }

            template <class ReducerLike>
            auto make_interval_tree_helper(std::vector<std::optional<double>>& interval_tree,
                                           size_t first, size_t last,
                                           size_t idx,
                                           ReducerLike&& reducer,
                                           const std::vector<double>& managing_interval) -> std::optional<double>
            {
                if (first + 1 == last)
                {
                    if (first < managing_interval.size())
                    {
                        interval_tree[idx] = managing_interval[first];
                        return interval_tree[idx];
                    }
                    else
                    {
                        interval_tree[idx] = std::nullopt;
                        return std::nullopt;
                    }
                }

                size_t interval_sz          = last - first;
                size_t mid_sz               = interval_sz / 2;
                size_t nxt_first            = first + mid_sz;

                std::optional<double> lhs   = this->make_interval_tree_helper(interval_tree,
                                                                              first, nxt_first,
                                                                              idx * 2 + 1,
                                                                              reducer,
                                                                              managing_interval);

                std::optional<double> rhs   = this->make_interval_tree_helper(interval_tree,
                                                                              nxt_first, last,
                                                                              idx * 2 + 2,
                                                                              reducer,
                                                                              managing_interval);

                if (lhs.has_value())
                {
                    if (rhs.has_value())
                    {
                        interval_tree[idx] = reducer(lhs.value(), rhs.value());
                    }
                    else
                    {
                        interval_tree[idx] = lhs.value();
                    }
                }
                else
                {
                    if (rhs.has_value())
                    {
                        interval_tree[idx] = rhs.value();
                    }
                    else
                    {
                        interval_tree[idx] = std::nullopt;
                    }
                }

                return interval_tree[idx];
            }

            template <class ReducerLike>
            auto make_interval_tree_from(const std::vector<double>& managing_interval,
                                         ReducerLike&& reducer,
                                         size_t base_sz) -> std::vector<std::optional<double>>
            {
                if (!stdx::is_pow2(base_sz))
                {
                    throw std::invalid_argument("bad base size, not pow 2 size");
                }

                if (managing_interval.size() > base_sz)
                {
                    throw std::invalid_argument("bad managing interval size, unfit tree size");
                }

                size_t tree_sz  = base_sz * 2 + 1;
                std::vector<std::optional<double>> interval_tree(tree_sz, std::nullopt);

                size_t first    = 0u;
                size_t last     = base_sz;
                size_t idx      = 0u;

                this->make_interval_tree_helper(interval_tree,
                                                first, last,
                                                idx,
                                                reducer,
                                                managing_interval);

                return interval_tree;
            }

            template <class ReducerLike>
            auto get_helper(const std::vector<std::optional<double>>& interval_tree,
                            size_t tree_first, size_t tree_last,
                            size_t key_first, size_t key_last,
                            size_t idx,
                            ReducerLike&& reducer) -> std::optional<double>
            {
                if (tree_first == key_first && tree_last == key_last)
                {
                    return interval_tree[idx];
                }

                size_t interval_sz  = tree_last - tree_first;
                size_t mid_sz       = interval_sz / 2;
                size_t nxt_first    = tree_first + mid_sz;

                if (key_last <= nxt_first)
                {
                    return this->get_helper(interval_tree,
                                            tree_first, nxt_first,
                                            key_first, key_last,
                                            idx * 2 + 1,
                                            reducer);
                }
                else if (key_first >= nxt_first)
                {
                    return this->get_helper(interval_tree,
                                            nxt_first, tree_last,
                                            key_first, key_last,
                                            idx * 2 + 2,
                                            reducer);
                }
                else
                {
                    std::optional<double> lhs = this->get_helper(interval_tree,
                                                                 tree_first, nxt_first,
                                                                 key_first, nxt_first,
                                                                 idx * 2 + 1,
                                                                 reducer);

                    std::optional<double> rhs = this->get_helper(interval_tree,
                                                                 nxt_first, tree_last,
                                                                 nxt_first, key_last,
                                                                 idx * 2 + 2,
                                                                 reducer);

                    if (lhs.has_value())
                    {
                        if (rhs.has_value())
                        {
                            return reducer(lhs.value(), rhs.value());
                        }
                        else
                        {
                            return lhs.value();
                        }
                    }
                    else
                    {
                        if (rhs.has_value())
                        {
                            return rhs.value();
                        }
                        else
                        {
                            return std::nullopt;
                        }
                    }
                }
            }
    };

    class MaxReducer
    {
        public:

            constexpr auto operator()(double lhs, double rhs) const -> double
            {
                stdx::safe_float_access(lhs);
                stdx::safe_float_access(rhs);

                return std::max(lhs, rhs);
            }            
    };

    class MinReducer
    {
        public:

            constexpr auto operator()(double lhs, double rhs) const -> double
            {
                stdx::safe_float_access(lhs);
                stdx::safe_float_access(rhs);

                return std::min(lhs, rhs);
            }
    };

    class SumReducer
    {
        public:

            constexpr auto operator()(double lhs, double rhs) const -> double
            {
                return lhs + rhs;
            }
    };

    class TemporalFeatureExtractor
    {
        public:

            static inline constexpr uint8_t FOCAL_UNIT_MICROSECOND                      = 0u;
            static inline constexpr uint8_t FOCAL_UNIT_MILLISECOND                      = 1u;
            static inline constexpr uint8_t FOCAL_UNIT_SECOND                           = 2u;
            static inline constexpr uint8_t FOCAL_UNIT_MINUTE                           = 3u;
            static inline constexpr uint8_t FOCAL_UNIT_DAY                              = 4u;

            static inline constexpr uint8_t FEATURIZATION_SECOND_ORDER_BINARY_SUFFIX    = 0u;
            static inline constexpr uint8_t FEATURIZATION_FIRST_ORDER_BINARY_SUFFIX     = 1u;

            static inline constexpr size_t MAX_DISCRETIZATION_SZ                        = 16u;

            static inline constexpr uint8_t ANALYTIC_SQUARE_DIFFERENCE                  = 0u;

            static inline constexpr uint8_t AGGREGATION_OPTION_MAX                      = 0u;
            static inline constexpr uint8_t AGGREGATION_OPTION_MIN                      = 1u;
            static inline constexpr uint8_t AGGREGATION_OPTION_AVG                      = 2u;
            static inline constexpr uint8_t AGGREGATION_OPTION_SUM                      = 3u;

            static inline constexpr double MAX_FOCAL_BASE                               = 20u;

        private:

            using epoch_t = uint64_t;

            struct FeatureTimePoint
            {
                epoch_t epoch_timepoint;
                double feature_value;
            };

            struct PrecomputedAccelerationResource
            {
                std::unique_ptr<IntervalReducedTree<MaxReducer>> max_reducer_resource;
                std::unique_ptr<IntervalReducedTree<MinReducer>> min_reducer_resource;
                std::unique_ptr<IntervalReducedTree<SumReducer>> sum_reducer_resource;
            };

            uint8_t focal_unit;
            double focal_base;
            size_t focal_sz;
            size_t focal_discretization_sz;
            uint8_t featurization_option;
            uint8_t analytic_option;

            std::vector<TickerData> ticker_data_vec;
            std::vector<std::string> feature_name_vec;
            std::unordered_map<std::string, std::unordered_map<std::string, std::vector<FeatureTimePoint>>> feature_map;
            std::unordered_map<std::string, std::unordered_map<std::string, PrecomputedAccelerationResource>> precomputed_map;
            std::unordered_map<std::string, uint8_t> feature_preferred_aggregation_map;

        public:

            TemporalFeatureExtractor(): focal_unit(FOCAL_UNIT_MINUTE),
                                        focal_base(10),
                                        focal_sz(8),
                                        focal_discretization_sz(8),
                                        featurization_option(FEATURIZATION_FIRST_ORDER_BINARY_SUFFIX),
                                        analytic_option(ANALYTIC_SQUARE_DIFFERENCE),
                                        ticker_data_vec(),
                                        feature_name_vec(),
                                        feature_map(),
                                        precomputed_map(),
                                        feature_preferred_aggregation_map(){}

            auto set_focal_unit(uint8_t focal_unit) -> TemporalFeatureExtractor&
            {
                switch (focal_unit)
                {
                    case FOCAL_UNIT_MICROSECOND:
                    case FOCAL_UNIT_MILLISECOND:
                    case FOCAL_UNIT_SECOND:
                    case FOCAL_UNIT_MINUTE:
                    case FOCAL_UNIT_DAY:
                    {
                        this->focal_unit = focal_unit;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad focal unit, no focal unit matched");
                    }
                }

                return *this;
            }

            auto set_focal_exponential_base(double focal_base) -> TemporalFeatureExtractor&
            {
                if (std::isnan(focal_base))
                {
                    throw std::invalid_argument("bad focal base, NaN");
                }

                if (focal_base < 0)
                {
                    throw std::invalid_argument("bad focal base, negative");
                }

                if (focal_base > MAX_FOCAL_BASE)
                {
                    throw std::invalid_argument("bad focal base, max value reached");
                }

                this->focal_base = focal_base;

                return *this;
            }

            auto set_focal_step(size_t sz) -> TemporalFeatureExtractor&
            {
                this->focal_sz = sz;

                return *this;
            }

            auto set_featurization_option(uint8_t featurization_option) -> TemporalFeatureExtractor&
            {
                switch (featurization_option)
                {
                    case FEATURIZATION_SECOND_ORDER_BINARY_SUFFIX:
                    case FEATURIZATION_FIRST_ORDER_BINARY_SUFFIX:
                    {
                        this->featurization_option = featurization_option;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad featurization option, enumeration out of range");
                    }
                }

                return *this;
            }

            auto set_analytic_option(uint8_t analytic_option) -> TemporalFeatureExtractor&
            {
                switch (analytic_option)
                {
                    case ANALYTIC_SQUARE_DIFFERENCE:
                    {
                        this->analytic_option = analytic_option;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad analytic option, enumeration out of range");
                    }
                }

                return *this;
            }

            auto set_focal_discretization_size(size_t sz) -> TemporalFeatureExtractor&
            {
                if (sz == 0u)
                {
                    throw std::invalid_argument("bad discretization size, 0");
                }

                if (sz > MAX_DISCRETIZATION_SZ)
                {
                    throw std::invalid_argument("bad discretization size, max discretization size reached");
                }

                this->focal_discretization_sz = sz;

                return *this;
            }

            auto set_data(const std::vector<TickerData>& data) -> TemporalFeatureExtractor&
            {
                for (const TickerData& ticker_point: data)
                {
                    if (std::isnan(ticker_point.feature_value))
                    {
                        throw std::invalid_argument("bad feature value, NaN");
                    }

                    if (std::isinf(ticker_point.feature_value))
                    {
                        throw std::invalid_argument("bad feature value, inf");
                    }
                }

                this->ticker_data_vec = data;

                return *this;
            }

            auto set_feature_name_list(const std::vector<std::string>& feature_name_vec) -> TemporalFeatureExtractor&
            {
                std::unordered_set<std::string> feature_name_set(feature_name_vec.begin(), feature_name_vec.end());
                this->feature_name_vec = std::vector<std::string>(feature_name_set.begin(), feature_name_set.end());

                return *this;
            }

            auto set_feature_aggregation_technique(const std::string& feature_name, uint8_t aggregation_option) -> TemporalFeatureExtractor&
            {
                switch (aggregation_option)
                {
                    case AGGREGATION_OPTION_MAX:
                    case AGGREGATION_OPTION_MIN:
                    case AGGREGATION_OPTION_AVG:
                    {
                        this->feature_preferred_aggregation_map[feature_name] = aggregation_option;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad aggregation option, enumeration out of range");
                    }
                }

                return *this;
            }

            auto set_feature_aggregation_technique_map(const std::unordered_map<std::string, uint8_t>& aggregation_option_map) -> TemporalFeatureExtractor&
            {
                for (const auto& [key, value]: aggregation_option_map)
                {
                    this->set_feature_aggregation_technique(key, value);
                }

                return *this;
            }

            auto compute() -> TemporalFeatureExtractor&
            {
                this->feature_map.clear();

                std::unordered_map<std::string, std::unordered_map<std::string, std::map<epoch_t, double>>> intermediate_map{};

                for (const TickerData& ticker_data: this->ticker_data_vec)
                {
                    epoch_t lapse_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(ticker_data.timestamp.time_since_epoch()).count();

                    if (std::isnan(lapse_since_epoch))
                    {
                        throw std::runtime_error("computation went wrong, NaN");
                    }

                    intermediate_map[ticker_data.ticker_name][ticker_data.feature_name].insert({lapse_since_epoch, ticker_data.feature_value});
                }

                for (const auto& [ticker_name, ticker_data]: intermediate_map)
                {
                    for (const auto& [feature_name, feature_data]: ticker_data)
                    {
                        std::vector<double> feature_vec = {};

                        for (const auto& [epoch_timestamp, feature_value]: feature_data)
                        {
                            this->feature_map[ticker_name][feature_name].push_back(FeatureTimePoint{.epoch_timepoint    = epoch_timestamp,
                                                                                                    .feature_value      = feature_value});

                            feature_vec.push_back(feature_value);
                        }

                        this->precomputed_map[ticker_name][feature_name] = PrecomputedAccelerationResource
                        {
                            .max_reducer_resource   = std::make_unique<IntervalReducedTree<MaxReducer>>(feature_vec),
                            .min_reducer_resource   = std::make_unique<IntervalReducedTree<MinReducer>>(feature_vec),
                            .sum_reducer_resource   = std::make_unique<IntervalReducedTree<SumReducer>>(feature_vec)
                        };
                    }
                }

                return *this;
            }

            auto get_feature_vector_at_timepoint(const std::string& ticker_name,
                                                 std::chrono::time_point<std::chrono::system_clock> timepoint) -> std::vector<double>
            {
                std::unordered_map<std::string, std::vector<std::vector<std::optional<double>>>> raw_feature_map = this->get_raw_feature_vector_at_timepoint(ticker_name, timepoint);

                switch (this->featurization_option)
                {
                    case FEATURIZATION_SECOND_ORDER_BINARY_SUFFIX:
                    {
                        return this->second_order_binary_featurize(raw_feature_map);
                    }
                    case FEATURIZATION_FIRST_ORDER_BINARY_SUFFIX:
                    {
                        return this->first_order_binary_featurize(raw_feature_map);
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_raw_feature_vector_at_timepoint_x(const std::string& ticker_name,
                                                       std::chrono::time_point<std::chrono::system_clock> timepoint) -> std::unordered_map<std::string, std::vector<std::vector<std::optional<double>>>>
            {
                return this->get_raw_feature_vector_at_timepoint(ticker_name, timepoint);
            }

            auto analyze(const std::vector<double>& feature_vec) -> FeatureAnalyticReport
            {
                switch (this->analytic_option)
                {
                    case ANALYTIC_SQUARE_DIFFERENCE:
                    {
                        return this->sqrdiff_analyze(feature_vec);
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

        private:

            auto double_to_bool_vector(float value) -> std::vector<double>
            {
                static_assert(std::numeric_limits<float>::is_iec559);

                uint32_t bitvec         = std::bit_cast<uint32_t>(value);
                std::vector<double> rs  = {};

                for (size_t i = 0u; i < std::numeric_limits<uint32_t>::digits; ++i)
                {
                    uint32_t test_bit = uint32_t{1} << i;

                    if ((bitvec & test_bit) != 0u)
                    {
                        rs.push_back(1);
                    }
                    else
                    {
                        rs.push_back(0);
                    }
                }

                return rs;
            }

            auto hole_punch(size_t enumeration_idx, size_t enumeration_sz) -> std::vector<double>
            {
                if (enumeration_idx >= enumeration_sz)
                {
                    throw std::invalid_argument("bad enumeration index, out of range access");
                }

                std::vector<double> result(enumeration_sz, 0);
                result[enumeration_idx] = 1;

                return result;
            }

            auto get_unit_duration() -> std::chrono::nanoseconds
            {
                using namespace std::chrono;

                switch (this->focal_unit)
                {
                    case FOCAL_UNIT_MICROSECOND:
                    {
                        return duration_cast<nanoseconds>(microseconds(1));
                    }
                    case FOCAL_UNIT_MILLISECOND:
                    {
                        return duration_cast<nanoseconds>(milliseconds(1));
                    }
                    case FOCAL_UNIT_SECOND:
                    {
                        return duration_cast<nanoseconds>(seconds(1));
                    }
                    case FOCAL_UNIT_MINUTE:
                    {
                        return duration_cast<nanoseconds>(minutes(1));
                    }
                    case FOCAL_UNIT_DAY:
                    {
                        return duration_cast<nanoseconds>(days(1));
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_timeslice_vector_since(std::chrono::time_point<std::chrono::system_clock> timepoint) -> std::vector<std::vector<std::pair<epoch_t, epoch_t>>>
            {
                if (timepoint < decltype(timepoint)(typename decltype(timepoint)::duration(0)))
                {
                    throw std::invalid_argument("bad timepoint, negative timepoint");
                }

                std::vector<std::vector<std::pair<epoch_t, epoch_t>>> result{};
                std::chrono::nanoseconds unit_dur = this->get_unit_duration();

                for (size_t i = 0u; i < this->focal_sz; ++i)
                {
                    std::vector<std::pair<epoch_t, epoch_t>> window_vec = {};
                    double focal_value                                  = std::pow(this->focal_base, i);

                    auto first                                          = timepoint - (unit_dur * focal_value);
                    auto last                                           = timepoint;

                    if (first < decltype(first)(typename decltype(first)::duration(0)))
                    {
                        first = decltype(first)(typename decltype(first)::duration(0));
                    }

                    uint64_t first_epoch                                = std::chrono::duration_cast<std::chrono::nanoseconds>(first.time_since_epoch()).count();
                    uint64_t last_epoch                                 = std::chrono::duration_cast<std::chrono::nanoseconds>(last.time_since_epoch()).count();
                    uint64_t lapse                                      = last_epoch - first_epoch;
                    uint64_t interval_sz                                = lapse / this->focal_discretization_sz;

                    for (size_t j = 0u; j < this->focal_discretization_sz; ++j)
                    {
                        uint64_t tentative_first    = first_epoch + j * interval_sz;
                        uint64_t tentative_last     = first_epoch + (j + 1) * interval_sz;

                        window_vec.push_back({tentative_first, tentative_last});
                    }

                    if (window_vec.empty())
                    {
                        std::abort();
                    }

                    window_vec.back().second = last_epoch;
                    result.push_back(std::move(window_vec));

                }

                return result;
            }

            //front() < x <= back()
            //return back()

            auto binary_seek_first_helper(FeatureTimePoint * first, FeatureTimePoint * last, epoch_t epoch_timepoint) -> FeatureTimePoint *
            {
                size_t sz = std::distance(first, last);

                if (sz == 2u)
                {
                    return std::next(first);
                }

                size_t mid_sz                   = sz / 2;
                FeatureTimePoint * nxt_point    = std::next(first, mid_sz);

                if (epoch_timepoint <= nxt_point->epoch_timepoint)
                {
                    return this->binary_seek_first_helper(first, std::next(nxt_point), epoch_timepoint);
                }

                return this->binary_seek_first_helper(nxt_point, last, epoch_timepoint);
            }

            auto binary_seek_first(FeatureTimePoint * first, FeatureTimePoint * last, epoch_t epoch_timepoint) -> FeatureTimePoint *
            {
                intmax_t chk_sz = std::distance(first, last);

                if (chk_sz < 0)
                {
                    std::abort();
                }
                
                if (std::isnan(epoch_timepoint))
                {
                    throw std::invalid_argument("bad epoch timepoint, NaN");
                }

                size_t sz       = chk_sz;

                if (sz == 0u)
                {
                    return last;
                }

                if (first->epoch_timepoint >= epoch_timepoint)
                {
                    return first;
                }

                if (epoch_timepoint > std::prev(last)->epoch_timepoint)
                {
                    return last;
                }

                if (sz < 2u)
                {
                    std::abort();
                }

                return this->binary_seek_first_helper(first, last, epoch_timepoint);                
            }

            auto binary_interval(FeatureTimePoint * first,
                                 FeatureTimePoint * last,
                                 epoch_t epoch_timepoint_first,
                                 epoch_t epoch_timepoint_last) -> std::pair<std::add_pointer_t<FeatureTimePoint>, std::add_pointer_t<FeatureTimePoint>>
            {
                FeatureTimePoint * finding_first    = this->binary_seek_first(first, last, epoch_timepoint_first);
                FeatureTimePoint * finding_last     = this->binary_seek_first(first, last, epoch_timepoint_last);
                intmax_t tentative_sz               = std::distance(finding_first, finding_last);
                intmax_t sz                         = std::max(intmax_t{0}, tentative_sz);

                return std::make_pair(finding_first, std::next(finding_first, sz));
            }

            template <class Iterator, class Extractor>
            auto reduce(uint8_t best_metric,
                        Iterator first, Iterator last,
                        Extractor&& extractor) -> std::decay_t<decltype(extractor(*first))>
            {
                auto resolutor = [&]<class Reducer>(Reducer&& reducer) -> std::decay_t<decltype(extractor(*first))>
                {
                    if (std::distance(first, last) == 0u)
                    {
                        throw std::invalid_argument("bad reduce, invalid size");
                    }

                    auto first_value = extractor(*first);

                    for (auto it = std::next(first); it != last; std::advance(it, 1u))
                    {
                        first_value = reducer(first_value, extractor(*it));
                    }

                    return first_value;
                };

                switch (best_metric)
                {
                    case AGGREGATION_OPTION_MAX:
                    {
                        auto reducer = [](const auto& lhs, const auto& rhs)
                        {
                            if (rhs > lhs)
                            {
                                return rhs;
                            }
                            else
                            {
                                return lhs;
                            }
                        };

                        return resolutor(reducer);
                    }
                    case AGGREGATION_OPTION_MIN:
                    {
                        auto reducer = [](const auto& lhs, const auto& rhs)
                        {
                            if (rhs < lhs)
                            {
                                return rhs;
                            }
                            else
                            {
                                return lhs;
                            }
                        };

                        return resolutor(reducer);
                    }
                    case AGGREGATION_OPTION_AVG:
                    {
                        auto reducer = [](const auto& lhs, const auto& rhs)
                        {
                            return lhs + rhs;
                        };

                        return std::distance(first, last) == 0 ? std::decay_t<decltype(extractor(*first))>(resolutor(reducer))
                                                               : std::decay_t<decltype(extractor(*first))>(resolutor(reducer) / std::distance(first, last));
                    }
                    case AGGREGATION_OPTION_SUM:
                    {
                        auto reducer = [](const auto& lhs, const auto & rhs)
                        {
                            return lhs + rhs;
                        };

                        return resolutor(reducer);
                    }
                    default:
                    {
                        throw std::invalid_argument("bad metric, enumeration out of range");
                    }
                }
            }

            auto get_best_feature_for_window(const std::string& ticker_name,
                                             const std::string& feature_name,
                                             epoch_t epoch_timepoint_first,
                                             epoch_t epoch_timepoint_last) -> std::optional<double>
            {
                //this is complicated, let's leetcode this

                auto map_ptr = this->feature_map.find(ticker_name);

                if (map_ptr == this->feature_map.end())
                {
                    return std::nullopt;
                }

                auto map_ptr_2 = map_ptr->second.find(feature_name);

                if (map_ptr_2 == map_ptr->second.end())
                {
                    return std::nullopt;
                }

                uint8_t best_metric;

                if (auto metric_ptr = this->feature_preferred_aggregation_map.find(feature_name); metric_ptr != this->feature_preferred_aggregation_map.end())
                {
                    best_metric = metric_ptr->second;
                }
                else
                {
                    best_metric = AGGREGATION_OPTION_MIN;
                }

                FeatureTimePoint * tmp_first    = map_ptr_2->second.data();
                FeatureTimePoint * tmp_last     = std::next(tmp_first, map_ptr_2->second.size());

                auto [first, last]              = this->binary_interval(tmp_first, tmp_last, epoch_timepoint_first, epoch_timepoint_last);
                size_t first_idx                = std::distance(tmp_first, first);
                size_t last_idx                 = std::distance(tmp_first, last);

                if (std::distance(first, last) == 0u)
                {
                    return std::nullopt;
                }

                if (auto precomputed_map_ptr = this->precomputed_map.find(ticker_name); precomputed_map_ptr != this->precomputed_map.end())
                {
                    if (auto precomputed_map_ptr_2 = precomputed_map_ptr->second.find(feature_name); precomputed_map_ptr_2 != precomputed_map_ptr->second.end())
                    {
                        switch (best_metric)
                        {
                            case AGGREGATION_OPTION_MAX:
                            {
                                if (precomputed_map_ptr_2->second.max_reducer_resource != nullptr)
                                {
                                    return precomputed_map_ptr_2->second.max_reducer_resource->get({first_idx, last_idx - first_idx});
                                }

                                break;
                            }
                            case AGGREGATION_OPTION_MIN:
                            {
                                if (precomputed_map_ptr_2->second.min_reducer_resource != nullptr)
                                {
                                    return precomputed_map_ptr_2->second.min_reducer_resource->get({first_idx, last_idx - first_idx});
                                }

                                break;
                            }
                            case AGGREGATION_OPTION_AVG:
                            {
                                if (precomputed_map_ptr_2->second.sum_reducer_resource != nullptr)
                                {
                                    return precomputed_map_ptr_2->second.sum_reducer_resource->get({first_idx, last_idx - first_idx}) / (last_idx - first_idx);
                                }

                                break;
                            }
                            case AGGREGATION_OPTION_SUM:
                            {
                                if (precomputed_map_ptr_2->second.sum_reducer_resource != nullptr)
                                {
                                    return precomputed_map_ptr_2->second.sum_reducer_resource->get({first_idx, last_idx - first_idx});
                                }

                                break;
                            }
                            default:
                            {
                                std::abort();
                            }
                        }
                    }
                }

                return this->reduce(best_metric,
                                    first, last,
                                    [](const auto& obj){return obj.feature_value;});
            }

            auto get_raw_feature_vector_at_timepoint(const std::string& ticker_name,
                                                     std::chrono::time_point<std::chrono::system_clock> timepoint) -> std::unordered_map<std::string, std::vector<std::vector<std::optional<double>>>>
            {
                std::vector<std::vector<std::pair<epoch_t, epoch_t>>> timeslice_2d_vec              = this->get_timeslice_vector_since(timepoint);
                std::unordered_map<std::string, std::vector<std::vector<std::optional<double>>>> rs = {};

                for (const std::string& feature_name: this->feature_name_vec)
                {
                    std::vector<std::vector<std::optional<double>>> price_2d_vec = {};

                    for (const auto& timeslice_1d_vec: timeslice_2d_vec)
                    {
                        std::vector<std::optional<double>> price_1d_vec = {};

                        for (const auto& [first, last]: timeslice_1d_vec)
                        {
                            price_1d_vec.push_back(this->get_best_feature_for_window(ticker_name, feature_name, first, last));
                        }

                        price_2d_vec.push_back(std::move(price_1d_vec));
                    }

                    rs[feature_name] = std::move(price_2d_vec);
                }

                return rs;
            }

            auto to_non_optional_feature_vector(const std::vector<std::optional<double>>& feature_vector) -> std::vector<double>
            {
                std::vector<double> result_vec{};

                for (const std::optional<double>& feature: feature_vector)
                {
                    if (!feature.has_value())
                    {
                        result_vec.push_back(-1);
                    }
                    else
                    {
                        result_vec.push_back(feature.value());
                    }
                }

                return result_vec;
            }

            auto first_order_binary_featurize(const std::unordered_map<std::string, std::vector<std::vector<std::optional<double>>>>& raw_map) -> std::vector<double>
            {
                std::vector<double> feature_vec{};

                for (const auto& [feature_id, feature_2d_vec]: raw_map)
                {
                    for (const auto& feature_1d_vec: feature_2d_vec)
                    {
                        std::vector<double> unfancy_feature_1d_vec  = this->to_non_optional_feature_vector(feature_1d_vec);
                        auto [suffix_vec, nxt_vec]                  = SequenceCompressor{}.suffix_lossless_compress(unfancy_feature_1d_vec);

                        if (!nxt_vec.empty())
                        {
                            std::vector<double> tmp_vec = this->double_to_bool_vector(nxt_vec.front());
                            std::copy(tmp_vec.begin(), tmp_vec.end(), std::back_inserter(feature_vec));
                        }

                        for (size_t suffix: suffix_vec)
                        {
                            std::vector<double> tmp_vec = this->hole_punch(suffix, suffix_vec.size());
                            std::copy(tmp_vec.begin(), tmp_vec.end(), std::back_inserter(feature_vec));
                        }
                    }
                }

                return feature_vec;
            }

            auto second_order_binary_featurize(const std::unordered_map<std::string, std::vector<std::vector<std::optional<double>>>>& raw_map) -> std::vector<double>
            {
                std::vector<double> feature_vec{};

                for (const auto& [feature_id, feature_2d_vec]: raw_map)
                {
                    for (const auto& feature_1d_vec: feature_2d_vec)
                    {
                        std::vector<double> unfancy_feature_1d_vec  = this->to_non_optional_feature_vector(feature_1d_vec);
                        auto [suffix_vec, nxt_vec]                  = SequenceCompressor{}.suffix_lossless_compress(unfancy_feature_1d_vec);

                        if (!nxt_vec.empty())
                        {
                            std::vector<double> tmp_vec = this->double_to_bool_vector(nxt_vec.front());
                            std::copy(tmp_vec.begin(), tmp_vec.end(), std::back_inserter(feature_vec));
                        }

                        for (size_t suffix: suffix_vec)
                        {
                            std::vector<double> tmp_vec = this->hole_punch(suffix, suffix_vec.size());
                            std::copy(tmp_vec.begin(), tmp_vec.end(), std::back_inserter(feature_vec));
                        }

                        if (!nxt_vec.empty())
                        {
                            if (nxt_vec.size() > 1)
                            {
                                nxt_vec[0] = nxt_vec[1];
                            }

                            auto [suffix_vec_2, nxt_vec_2]  = SequenceCompressor{}.suffix_lossless_compress({nxt_vec.begin(), nxt_vec.end()});

                            if (!nxt_vec_2.empty())
                            {
                                std::vector<double> tmp_vec = this->double_to_bool_vector(nxt_vec_2.front());
                                std::copy(tmp_vec.begin(), tmp_vec.end(), std::back_inserter(feature_vec));
                            }

                            for (size_t suffix: suffix_vec_2)
                            {
                                std::vector<double> tmp_vec = this->hole_punch(suffix, suffix_vec_2.size());
                                std::copy(tmp_vec.begin(), tmp_vec.end(), std::back_inserter(feature_vec));
                            }
                        }
                    }
                }

                return feature_vec;
            }

            auto sqrdiff_analyze(const std::vector<double>& feature_vec) -> FeatureAnalyticReport
            {
                std::vector<std::vector<std::pair<epoch_t, epoch_t>>> timeslice_2d_vec = this->get_timeslice_vector_since({});
                size_t first = 0u;
                FeatureAnalyticReport report{};

                auto get_suffix_sentiment_score = [&](const std::vector<size_t>& suffix_vec)
                {
                    if (suffix_vec.size() <= 1)
                    {
                        return 0;
                    }

                    size_t back_suffix_idx      = suffix_vec[suffix_vec.size() - 1];
                    size_t prev_back_suffix_idx = suffix_vec[suffix_vec.size() - 2];

                    if (back_suffix_idx > prev_back_suffix_idx)
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                };

                auto sqrdiff_first_order_defeaturize = [&](const std::vector<double>& suffix_encoded_vec)
                {
                    size_t tentative_first          = this->double_to_bool_vector({}).size();
                    size_t actual_first             = std::min(suffix_encoded_vec.size(), tentative_first);
                    size_t suffix_feature_arr_sz    = suffix_encoded_vec.size() - actual_first;
                    size_t suffix_arr_sz            = std::sqrt(suffix_feature_arr_sz);

                    if (suffix_arr_sz != this->focal_discretization_sz)
                    {
                        throw std::runtime_error("sqrdiff calculation went wrong");
                    }

                    std::vector<size_t> suffix_vec  = {};
                    double confident_score          = 0;

                    for (size_t i = 0u; i < suffix_arr_sz; ++i)
                    {
                        size_t hole_first   = actual_first + i * suffix_arr_sz;
                        size_t hole_last    = hole_first + suffix_arr_sz;

                        if (hole_last > suffix_encoded_vec.size())
                        {
                            throw std::runtime_error("sqrdiff calculation went wrong");
                        }

                        size_t idx          = std::distance(std::next(feature_vec.begin(), hole_first),
                                                            std::max_element(std::next(feature_vec.begin(), hole_first), std::next(feature_vec.begin(), hole_last)));

                        suffix_vec.push_back(idx);

                        for (size_t j = hole_first; j < hole_last; ++j)
                        {
                            if (j == idx)
                            {
                                confident_score += std::pow(suffix_encoded_vec[j] - 1, 2) * i;
                            }
                            else
                            {
                                confident_score += std::pow(suffix_encoded_vec[j], 2) * i;
                            }
                        }
                    }
 
                    return std::make_pair(std::move(suffix_vec), confident_score);
                };

                auto sqrdiff_second_order_defeaturize = [&](const std::vector<double>& suffix_encoded_vec)
                {
                    size_t half_sz = suffix_encoded_vec.size() / 2;
                    std::vector<double> half_encoded_vec(suffix_encoded_vec.begin(), std::next(suffix_encoded_vec.begin(), half_sz));

                    return sqrdiff_first_order_defeaturize(half_encoded_vec);
                };

                auto sqrdiff_defeaturize = [&](const std::vector<double>& suffix_encoded_vec)
                {
                    switch (this->featurization_option)
                    {
                        case FEATURIZATION_FIRST_ORDER_BINARY_SUFFIX:
                        {
                            return sqrdiff_first_order_defeaturize(suffix_encoded_vec);
                        }
                        case FEATURIZATION_SECOND_ORDER_BINARY_SUFFIX:
                        {
                            return sqrdiff_second_order_defeaturize(suffix_encoded_vec);
                        }
                        default:
                        {
                            std::abort();
                        }
                    }
                };

                size_t chunk_sz = this->feature_name_vec.size() * timeslice_2d_vec.size();

                if (chunk_sz == 0u)
                {
                    return report;
                }

                size_t slice_sz = feature_vec.size() / chunk_sz;

                for (const std::string& feature_name: this->feature_name_vec)
                {
                    for (size_t i = 0u; i < timeslice_2d_vec.size(); ++i)
                    {
                        size_t focal_idx                    = i;
                        size_t last                         = first + slice_sz;

                        std::vector<double> score_vec(std::next(feature_vec.begin(), first), std::next(feature_vec.begin(), last));
                        first                               = last;

                        std::vector<size_t> suffix;
                        double confident_score;

                        std::tie(suffix, confident_score)   = sqrdiff_defeaturize(score_vec);
                        intmax_t suffix_sentiment_score     = get_suffix_sentiment_score(suffix);

                        report.analytic_point_vec.push_back
                        (
                            FeatureAnalyticPoint
                            {
                                .feature_id         = feature_name,
                                .focal_idx          = focal_idx,
                                .bear_or_bull       = static_cast<bool>(suffix_sentiment_score > 0),
                                .confident_score    = confident_score
                            }
                        );
                    }
                }

                return report;
            }
    };

    class OneOneMatrixEncoder
    {
        private:

            std::vector<size_t> matrix_shape;
            size_t logit_vec_sz;
            size_t matrix_vec_sz;

        public:

            OneOneMatrixEncoder(const std::vector<size_t>& matrix_shape,
                                size_t logit_vec_sz): matrix_shape(matrix_shape),
                                                      logit_vec_sz(logit_vec_sz)
            {
                size_t shape_sz = this->to_shape_size(matrix_shape);

                if (logit_vec_sz > shape_sz)
                {
                    throw std::invalid_argument("bad logit vec size, incompatible shape");
                }

                this->matrix_vec_sz = shape_sz;
            }

            auto encode(const std::vector<double>& feature_vec) -> std::shared_ptr<tensor_model::Matrix>
            {
                if (feature_vec.size() != this->logit_vec_sz)
                {
                    throw std::invalid_argument("bad feature vec size, mismatched configurated size");
                }

                std::vector<double> new_feature_vec = feature_vec;
                new_feature_vec.resize(this->matrix_vec_sz, 0);

                return tensor_matrix_operation::make_matrix_from_flat_vec(this->matrix_shape,
                                                                          std::vector<tensor_model::tensor_std_float_t>(stdx::to_castable_vector_initializer(new_feature_vec)));
            }

            auto decode(const std::shared_ptr<tensor_model::Matrix>& matrix) -> std::vector<double>
            {
                if (matrix == nullptr)
                {
                    throw std::invalid_argument("bad matrix, null");
                }

                std::vector<tensor_model::tensor_std_float_t> result{};
                tensor_matrix_operation::flatten(matrix, result);
                
                if (result.size() != this->matrix_vec_sz)
                {
                    throw std::invalid_argument("bad matrix, incompatible shape");
                }

                result.resize(this->logit_vec_sz);

                return stdx::to_castable_vector_initializer(result);
            }

        private:

            auto to_shape_size(const std::vector<size_t>& matrix_shape) -> size_t
            {
                if (matrix_shape.empty())
                {
                    return 0u;
                }

                return std::accumulate(matrix_shape.begin(), matrix_shape.end(), size_t{1}, std::multiplies<size_t>{});
            }
    };

    struct SolutionData
    {
        std::vector<std::string> extractor_ticker_vec;
        std::vector<std::string> extractor_feature_name_list;

        uint8_t extractor_focal_unit;
        double extractor_focal_exponential_base;
        uint64_t extractor_focal_step;
        uint8_t extractor_featurization_option;
        uint8_t extractor_analytic_option;
        uint64_t extractor_focal_discretization_sz;

        uint8_t matrix_maker_entropy_option;
        uint8_t matrix_maker_compute_option;
        uint64_t matrix_maker_vector_sz;

        std::vector<uint64_t> matrix_encoder_shape;
        uint64_t matrix_encoder_flat_sz;

        std::vector<tensor_model::tensor_std_float_t> matrix_logit_vec;

        std::chrono::time_point<std::chrono::system_clock> training_first;
        std::chrono::time_point<std::chrono::system_clock> training_last;
        std::chrono::nanoseconds training_lapse;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(extractor_ticker_vec, extractor_feature_name_list,
                      extractor_focal_unit, extractor_focal_exponential_base,
                      extractor_focal_step, extractor_featurization_option,
                      extractor_analytic_option, extractor_focal_discretization_sz,
                      matrix_maker_entropy_option, matrix_maker_compute_option,
                      matrix_maker_vector_sz,
                      matrix_encoder_shape, matrix_encoder_flat_sz,
                      matrix_logit_vec,
                      training_first, training_last,
                      training_lapse);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(extractor_ticker_vec, extractor_feature_name_list,
                      extractor_focal_unit, extractor_focal_exponential_base,
                      extractor_focal_step, extractor_featurization_option,
                      extractor_analytic_option, extractor_focal_discretization_sz,
                      matrix_maker_entropy_option, matrix_maker_compute_option,
                      matrix_maker_vector_sz,
                      matrix_encoder_shape, matrix_encoder_flat_sz,
                      matrix_logit_vec,
                      training_first, training_last,
                      training_lapse);
        }
    };

    class SolutionMaker
    {
        public:

            static inline constexpr uint8_t LOW_COMPUTE                 = 0u;
            static inline constexpr uint8_t MID_COMPUTE                 = 1u;
            static inline constexpr uint8_t HIGH_COMPUTE                = 2u;

            static inline constexpr uint8_t LOW_STORAGE                 = 0u;
            static inline constexpr uint8_t MID_STORAGE                 = 1u;
            static inline constexpr uint8_t HIGH_STORAGE                = 2u;

            static inline constexpr uint8_t LOW_TRAIN                   = 0u;
            static inline constexpr uint8_t MID_TRAIN                   = 1u;
            static inline constexpr uint8_t HIGH_TRAIN                  = 2u;

            static inline const std::chrono::nanoseconds DEFAULT_WINDOW = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::years(1));
            static inline const std::chrono::nanoseconds DEFAULT_LAPSE  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::days(1));

        private:

            struct TrainingDataPoint
            {
                std::vector<double> in_point_vec;
                std::vector<double> out_point_vec;
            };

            struct TrainingData
            {
                std::vector<TrainingDataPoint> point_vec;
            };

            uint8_t compute_option;
            uint8_t storage_option;
            uint8_t train_option;

            std::vector<TickerData> ticker_data_vec;
            std::vector<std::string> ticker_vec;
            std::vector<std::string> feature_name_vec;

            std::chrono::time_point<std::chrono::system_clock> first;
            std::chrono::time_point<std::chrono::system_clock> last;
            std::chrono::nanoseconds lapse;

        public:

            SolutionMaker(): compute_option(LOW_COMPUTE),
                             storage_option(LOW_STORAGE),
                             ticker_data_vec(),
                             ticker_vec(),
                             feature_name_vec(),
                             first(get_past(DEFAULT_WINDOW)),
                             last(get_now()),
                             lapse(DEFAULT_LAPSE){}

            auto set_training(uint8_t option) -> SolutionMaker&
            {
                switch (option)
                {
                    case LOW_TRAIN:
                    case MID_TRAIN:
                    case HIGH_TRAIN:
                    {
                        this->train_option = option;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad option, enumeration out of range");
                    }
                }

                return *this;
            }

            auto set_compute(uint8_t option) -> SolutionMaker&
            {
                switch (option)
                {
                    case LOW_COMPUTE:
                    case MID_COMPUTE:
                    case HIGH_COMPUTE:
                    {
                        this->compute_option = option;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad option, enumeration out of range");
                    }
                }

                return *this;
            }

            auto set_storage(uint8_t option) -> SolutionMaker&
            {
                switch (option)
                {
                    case LOW_STORAGE:
                    case MID_STORAGE:
                    case HIGH_STORAGE:
                    {
                        this->storage_option = option;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad option, enumeration out of range");
                    }
                }

                return *this;
            }

            auto set_data(const std::vector<TickerData>& data) -> SolutionMaker&
            {
                this->ticker_data_vec = data;

                return *this;
            }

            auto set_training_first_timepoint(std::chrono::time_point<std::chrono::system_clock> timepoint) -> SolutionMaker&
            {
                this->first = timepoint;

                return *this;
            }

            auto set_training_last_timepoint(std::chrono::time_point<std::chrono::system_clock> timepoint) -> SolutionMaker&
            {
                this->last = timepoint;

                return *this;
            }

            auto set_feature_name_list(const std::vector<std::string>& feature_vec) -> SolutionMaker&
            {
                this->feature_name_vec = feature_vec;

                return *this;
            }

            auto set_tickers(const std::vector<std::string>& ticker_vec) -> SolutionMaker&
            {
                this->ticker_vec = ticker_vec;

                return *this;
            }

            auto set_training_intput_output_lapse(std::chrono::nanoseconds dur) -> SolutionMaker&
            {
                if (dur <= std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad lapse, negative or zero");
                }

                this->lapse = dur;

                return *this;
            }

            auto train() -> SolutionData
            {
                std::unique_ptr<the_matrix::MatrixInterface> matrix                                                                 = this->get_transform_matrix();
                std::unique_ptr<matrix_optimizer::MatrixOptimizerInterface> optimizer                                               = this->get_optimizer();
                std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> training_data  = this->to_matrix_training_data(this->get_training_data());

                optimizer->optimize(*matrix, training_data);

                return SolutionData
                {
                    .extractor_ticker_vec               = this->ticker_vec,
                    .extractor_feature_name_list        = this->feature_name_vec,

                    .extractor_focal_unit               = this->get_focal_unit(),
                    .extractor_focal_exponential_base   = this->get_focal_exponential_base(),
                    .extractor_focal_step               = stdx::throw_integer_cast<uint64_t>(this->get_focal_step()),
                    .extractor_featurization_option     = this->get_featurization_option(),
                    .extractor_analytic_option          = this->get_analytic_option(),
                    .extractor_focal_discretization_sz  = stdx::throw_integer_cast<uint64_t>(this->get_focal_discretization_size()),

                    .matrix_maker_entropy_option        = this->get_matrix_maker_entropy_option(),
                    .matrix_maker_compute_option        = this->get_matrix_maker_compute_option(),
                    .matrix_maker_vector_sz             = stdx::throw_integer_cast<uint64_t>(this->get_matrix_maker_vector_size()),

                    .matrix_encoder_shape               = stdx::to_castable_vector_initializer(this->get_encoder_shape()),
                    .matrix_encoder_flat_sz             = stdx::throw_integer_cast<uint64_t>(this->get_encoder_flat_size()),

                    .matrix_logit_vec                   = matrix->get_coefficient_vector(),

                    .training_first                     = this->first,
                    .training_last                      = this->last,
                    .training_lapse                     = this->lapse
                };
            }

        private:

            auto get_size_from_shape(const std::vector<size_t>& shape) -> size_t
            {
                if (shape.empty())
                {
                    return 0u;
                }

                return std::accumulate(shape.begin(), shape.end(), size_t{1}, std::multiplies<size_t>{});
            }

            auto get_matrix_maker_entropy_option() -> uint8_t
            {
                return the_host_matrix::TheHostMatrixFactory::LOW_ENTROPY;
            }

            auto get_matrix_maker_compute_option() -> uint8_t
            {
                switch (this->compute_option)
                {
                    case LOW_COMPUTE:
                    {
                        return the_host_matrix::TheHostMatrixFactory::LOW_COMPUTE;
                    }
                    case MID_COMPUTE:
                    {
                        return the_host_matrix::TheHostMatrixFactory::MID_COMPUTE;
                    }
                    case HIGH_COMPUTE:
                    {
                        return the_host_matrix::TheHostMatrixFactory::HIGH_COMPUTE;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_matrix_maker_vector_size() -> size_t
            {
                return this->get_size_from_shape(this->get_encoder_shape());
            }

            auto get_encoder_shape() -> std::vector<size_t>
            {
                return the_host_matrix::TheHostMatrixFactory{}.set_entropy(this->get_matrix_maker_entropy_option())
                                                              .set_compute(this->get_matrix_maker_compute_option())
                                                              .set_vector_size(this->get_encoder_flat_size())
                                                              .get_matrix_shape();
            }

            auto get_encoder_flat_size() -> size_t
            {
                std::vector<double> feature_matrix = get_feature_extractor().get_feature_vector_at_timepoint({}, {});

                return feature_matrix.size();
            }

            static auto get_now() -> std::chrono::time_point<std::chrono::system_clock>
            {
                return std::chrono::system_clock::now();
            }

            static auto get_past(std::chrono::nanoseconds duration) -> std::chrono::time_point<std::chrono::system_clock>
            {
                auto new_timepoint      = get_now() - duration;
                using default_dur_rep_t = typename std::chrono::time_point<std::chrono::system_clock>::duration;

                return std::chrono::time_point_cast<default_dur_rep_t>(new_timepoint);
            }

            auto get_transform_matrix() -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                return the_host_matrix::TheHostMatrixFactory{}.set_compute(this->get_matrix_maker_compute_option())
                                                              .set_entropy(this->get_matrix_maker_entropy_option())
                                                              .set_vector_size(this->get_matrix_maker_vector_size())
                                                              .get();
            }

            auto get_optimizer_optimization_epoch_size() -> size_t
            {
                const size_t LOW_COMPUTE_EPOCH_SZ   = size_t{1} << 16;
                const size_t MID_COMPUTE_EPOCH_SZ   = size_t{1} << 20;
                const size_t HIGH_COMPUTE_EPOCH_SZ  = size_t{1} << 24;

                switch (this->compute_option)
                {
                    case LOW_COMPUTE:
                    {
                        return LOW_COMPUTE_EPOCH_SZ;
                    }
                    case MID_COMPUTE:
                    {
                        return MID_COMPUTE_EPOCH_SZ;
                    }
                    case HIGH_COMPUTE:
                    {
                        return HIGH_COMPUTE_EPOCH_SZ;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_optimizer_optimization_step_size() -> size_t
            {
                const size_t LOW_COMPUTE_OPTIMIZATION_STEP_SZ   = size_t{1} << 4;
                const size_t MID_COMPUTE_OPTIMIZATION_STEP_SZ   = size_t{1} << 4;
                const size_t HIGH_COMPUTE_OPTIMIZATION_STEP_SZ  = size_t{1} << 4;

                switch (this->compute_option)
                {
                    case LOW_COMPUTE:
                    {
                        return LOW_COMPUTE_OPTIMIZATION_STEP_SZ;
                    }
                    case MID_COMPUTE:
                    {
                        return MID_COMPUTE_OPTIMIZATION_STEP_SZ;
                    }
                    case HIGH_COMPUTE:
                    {
                        return HIGH_COMPUTE_OPTIMIZATION_STEP_SZ;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_optimizer_space_iteration_size() -> size_t
            {
                const size_t LOW_COMPUTE_SPACE_ITERATION_SZ     = size_t{1} << 4;
                const size_t MID_COMPUTE_SPACE_ITERATION_SZ     = size_t{1} << 4;
                const size_t HIGH_COMPUTE_SPACE_ITERATION_SZ    = size_t{1} << 4;

                switch (this->compute_option)
                {
                    case LOW_COMPUTE:
                    {
                        return LOW_COMPUTE_SPACE_ITERATION_SZ;
                    }
                    case MID_COMPUTE:
                    {
                        return MID_COMPUTE_SPACE_ITERATION_SZ;
                    }
                    case HIGH_COMPUTE:
                    {
                        return HIGH_COMPUTE_SPACE_ITERATION_SZ;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_optimizer() -> std::unique_ptr<matrix_optimizer::MatrixOptimizerInterface>
            {
                matrix_optimizer::BruteForceMatrixOptimizer matrix_optimizer{};

                matrix_optimizer.set_optimization_epoch_size(this->get_optimizer_optimization_epoch_size())
                                .set_optimization_step_size(this->get_optimizer_optimization_step_size())
                                .set_space_iteration_size(this->get_optimizer_space_iteration_size());

                return std::make_unique<matrix_optimizer::BruteForceMatrixOptimizer>(std::move(matrix_optimizer));
            }

            auto get_feature_extractor() -> TemporalFeatureExtractor
            {
                TemporalFeatureExtractor extractor{};

                extractor.set_focal_unit(this->get_focal_unit())
                         .set_focal_exponential_base(this->get_focal_exponential_base())
                         .set_focal_step(this->get_focal_step())
                         .set_featurization_option(this->get_featurization_option())
                         .set_focal_discretization_size(this->get_focal_discretization_size())
                         .set_analytic_option(this->get_analytic_option())
                         .set_data(this->ticker_data_vec)
                         .set_feature_name_list(this->feature_name_vec)
                         .compute();
                
                return extractor;
            }

            auto get_training_data() -> TrainingData
            {
                if (this->last < this->first)
                {
                    throw std::invalid_argument("bad time slice, negative time slice");
                }


                TemporalFeatureExtractor extractor  = this->get_feature_extractor();
                size_t epoch_first                  = std::chrono::duration_cast<std::chrono::nanoseconds>(this->first.time_since_epoch()).count();
                size_t epoch_last                   = std::chrono::duration_cast<std::chrono::nanoseconds>(this->last.time_since_epoch()).count();
                size_t time_interval_uint           = std::chrono::duration_cast<std::chrono::nanoseconds>(this->last - this->first).count();
                size_t lapse_uint                   = this->lapse.count();

                if (lapse_uint == 0u)
                {
                    throw std::runtime_error("unexpected error, time lapse 0");
                }

                size_t slice_sz             = time_interval_uint / lapse_uint + static_cast<size_t>(time_interval_uint % lapse_uint != 0u);
                TrainingData result         = {};

                for (size_t i = 0u; i < slice_sz; ++i)
                {
                    size_t local_epoch_first        = epoch_first + i * lapse_uint;
                    size_t local_epoch_last         = std::min(static_cast<size_t>(local_epoch_first + lapse_uint), epoch_last);

                    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> time_point_first{std::chrono::nanoseconds(local_epoch_first)}; //what is going on??
                    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> time_point_last{std::chrono::nanoseconds(local_epoch_last)};

                    using default_dur_rep_t         = typename std::chrono::time_point<std::chrono::system_clock>::duration;

                    std::vector<double> in_feature  = this->extract_feature_at(extractor, std::chrono::time_point_cast<default_dur_rep_t>(time_point_first));
                    std::vector<double> out_feature = this->extract_feature_at(extractor, std::chrono::time_point_cast<default_dur_rep_t>(time_point_last));

                    result.point_vec.push_back
                    (
                        TrainingDataPoint
                        {
                            .in_point_vec   = std::move(in_feature),
                            .out_point_vec  = std::move(out_feature)
                        }
                    );
                }

                return result;
            }

            auto to_matrix_training_data(const TrainingData& data) -> std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>
            {
                OneOneMatrixEncoder encoder(this->get_encoder_shape(), this->get_encoder_flat_size());
                std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> result{};

                for (const auto& data_point: data.point_vec)
                {
                    result.push_back({encoder.encode(data_point.in_point_vec),
                                      encoder.encode(data_point.out_point_vec)});
                }

                return result;
            }

            auto get_focal_unit() -> uint8_t
            {
                switch (this->compute_option)
                {
                    case LOW_COMPUTE:
                    {
                        return TemporalFeatureExtractor::FOCAL_UNIT_MINUTE;
                    }
                    case MID_COMPUTE:
                    {
                        return TemporalFeatureExtractor::FOCAL_UNIT_SECOND;
                    }
                    case HIGH_COMPUTE:
                    {
                        return TemporalFeatureExtractor::FOCAL_UNIT_SECOND;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_focal_exponential_base() -> double
            {
                switch (this->compute_option)                
                {
                    case LOW_COMPUTE:
                    {
                        return 10;
                    }
                    case MID_COMPUTE:
                    {
                        return 8;
                    }
                    case HIGH_COMPUTE:
                    {
                        return 5;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_focal_step() -> size_t
            {
                switch (this->compute_option)
                {
                    case LOW_COMPUTE:
                    {
                        return 6;
                    }
                    case MID_COMPUTE:
                    {
                        return 9;
                    }
                    case HIGH_COMPUTE:
                    {
                        return 12;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_featurization_option() -> uint8_t
            {
                switch (this->compute_option)
                {
                    case LOW_COMPUTE:
                    {
                        return TemporalFeatureExtractor::FEATURIZATION_FIRST_ORDER_BINARY_SUFFIX;
                    }
                    case MID_COMPUTE:
                    {
                        return TemporalFeatureExtractor::FEATURIZATION_SECOND_ORDER_BINARY_SUFFIX;
                    }
                    case HIGH_COMPUTE:
                    {
                        return TemporalFeatureExtractor::FEATURIZATION_SECOND_ORDER_BINARY_SUFFIX;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto get_analytic_option() -> uint8_t
            {
                return TemporalFeatureExtractor::ANALYTIC_SQUARE_DIFFERENCE;
            }

            auto get_focal_discretization_size() -> size_t
            {
                switch (this->compute_option)
                {
                    case LOW_COMPUTE:
                    {
                        return 4;
                    }
                    case MID_COMPUTE:
                    {
                        return 6;
                    }
                    case HIGH_COMPUTE:
                    {
                        return 6;
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto extract_feature_at(TemporalFeatureExtractor& extractor,
                                    std::chrono::time_point<std::chrono::system_clock> timepoint) -> std::vector<double>
            {
                std::vector<double> result{};

                for (const std::string& ticker: this->ticker_vec)
                {
                    std::vector<double> ticker_feature = extractor.get_feature_vector_at_timepoint(ticker, timepoint);
                    std::copy(ticker_feature.begin(), ticker_feature.end(), std::back_inserter(result));
                }

                return result;
            }
    };

    static inline const std::string ACTIONABLE_BUYSELL_NEXT_SECOND      = "buysell_next_second";
    static inline const std::string ACTIONABLE_BUYSELL_NEXT_MILLISECOND = "buysell_next_millisecond";
    static inline const std::string ACTIONABLE_SELLBUY_NEXT_SECOND      = "sellbuy_next_second";
    static inline const std::string ACTIONABLE_SELLBUY_NEXT_MILLISECOND = "sellbuy_next_millisecond";

    struct Actionable
    {
        std::string ticker_name;
        std::string actionable_type;
        double confident_score;
    };

    class SolutionProduct
    {
        private:

            SolutionData data;

            std::unique_ptr<TemporalFeatureExtractor> extractor;
            std::unique_ptr<the_matrix::MatrixInterface> transformer;
            std::unique_ptr<OneOneMatrixEncoder> encoder;

        public:

            SolutionProduct(const SolutionData& data): data(data)
            {
                this->extractor     = std::make_unique<TemporalFeatureExtractor>();

                this->extractor->set_focal_unit(data.extractor_focal_unit)
                                .set_focal_exponential_base(data.extractor_focal_exponential_base)
                                .set_focal_step(data.extractor_focal_step)
                                .set_focal_discretization_size(data.extractor_focal_discretization_sz)
                                .set_featurization_option(data.extractor_featurization_option)
                                .set_feature_name_list(data.extractor_feature_name_list)
                                .set_analytic_option(data.extractor_analytic_option);

                this->transformer   = the_host_matrix::TheHostMatrixFactory{}.set_compute(data.matrix_maker_compute_option)
                                                                             .set_entropy(data.matrix_maker_entropy_option)
                                                                             .set_vector_size(data.matrix_maker_vector_sz)
                                                                             .get();

                this->encoder       = std::make_unique<OneOneMatrixEncoder>(stdx::to_castable_vector_initializer(data.matrix_encoder_shape), data.matrix_encoder_flat_sz);
            }

            auto load_data(const std::vector<TickerData>& ticker_data) -> SolutionProduct&
            {
                this->extractor->set_data(ticker_data).compute();

                return *this;
            }

            auto predict(std::chrono::time_point<std::chrono::system_clock> forecast_timepoint) -> std::vector<Actionable>
            {
                const std::vector<std::string>& tickers                         = this->data.extractor_ticker_vec;

                std::vector<std::vector<double>> org_state_vec                  = this->featurize_tickers(tickers, forecast_timepoint);
                std::vector<double> unified_state                               = this->unify_state(org_state_vec);
                std::shared_ptr<tensor_model::Matrix> matrix_state              = this->encoder->encode(unified_state);
                std::shared_ptr<tensor_model::Matrix> transformed_matrix_state  = this->transformer->project({matrix_state}).front();
                std::vector<double> transformed_state                           = this->encoder->decode(transformed_matrix_state);
                std::vector<std::vector<double>> predicted_state_vec            = this->individualize_state_as(transformed_state, org_state_vec);
                std::vector<Actionable> actionable_vec                          = {};

                for (const auto& [ticker, state]: stdx::zip(tickers, predicted_state_vec))
                {
                    FeatureAnalyticReport report = this->extractor->analyze(state);

                    for (const FeatureAnalyticPoint& analytic_point: report.analytic_point_vec)
                    {
                        std::optional<std::string> actionable   = this->get_actionable(analytic_point);

                        if (!actionable.has_value())
                        {
                            continue;
                        }

                        actionable_vec.push_back
                        (
                            Actionable
                            {
                                .ticker_name        = ticker,
                                .actionable_type    = actionable.value(),
                                .confident_score    = analytic_point.confident_score
                            }
                        );
                    }
                }

                return actionable_vec;
            }

        private:

            auto featurize_tickers(const std::vector<std::string>& ticker_vec,
                                   std::chrono::time_point<std::chrono::system_clock> forecast_timepoint) -> std::vector<std::vector<double>>
            {
                std::vector<std::vector<double>> result_vec{};

                for (const std::string& ticker: ticker_vec)
                {
                    std::vector<double> feature_vec = this->extractor->get_feature_vector_at_timepoint(ticker, forecast_timepoint);
                    result_vec.push_back(std::move(feature_vec));
                }

                return result_vec;
            }

            auto unify_state(const std::vector<std::vector<double>>& state_vec) -> std::vector<double>
            {
                std::vector<double> result_vec{};

                for (const std::vector<double>& state: state_vec)
                {
                    std::copy(state.begin(), state.end(), std::back_inserter(result_vec));
                }

                return result_vec;
            }

            auto individualize_state_as(const std::vector<double>& state_vec,
                                        const std::vector<std::vector<double>>& shape_vec) -> std::vector<std::vector<double>>
            {
                std::vector<std::vector<double>> result_vec{};
                size_t first = 0u;

                for (const auto& shape: shape_vec)
                {
                    size_t last = first + shape.size();

                    if (last > state_vec.size())
                    {
                        throw std::invalid_argument("incompatible shape, out of range access");
                    }

                    result_vec.push_back({std::next(state_vec.begin(), first),
                                          std::next(state_vec.begin(), last)});

                    first = last;
                }

                return result_vec;
            }

            auto is_next_millisecond_system() -> bool
            {
                return true;
            }

            auto is_next_second_system() -> bool
            {
                return true;
            }

            auto is_in_millisecond_range(const FeatureAnalyticPoint& analytic_point) -> bool
            {
                return true;
            }

            auto is_in_second_range(const FeatureAnalyticPoint& analytic_point) -> bool
            {
                return true;
            }

            auto is_buyable_action(const FeatureAnalyticPoint& analytic_point) -> bool
            {
                return true;
            }

            auto is_sellable_action(const FeatureAnalyticPoint& analytic_point) -> bool
            {
                return true;
            }

            auto get_actionable(const FeatureAnalyticPoint& analytic_point) -> std::optional<std::string>
            {
                if (this->is_next_millisecond_system())
                {
                    if (this->is_in_millisecond_range(analytic_point))
                    {
                        if (this->is_buyable_action(analytic_point))
                        {
                            return ACTIONABLE_BUYSELL_NEXT_MILLISECOND;
                        }
                        else if (this->is_sellable_action(analytic_point))
                        {
                            return ACTIONABLE_SELLBUY_NEXT_MILLISECOND;
                        }
                        else
                        {
                            return std::nullopt;
                        }
                    }
                    else
                    {
                        return std::nullopt;
                    }
                }
                else if (this->is_next_second_system())
                {
                    if (this->is_in_second_range(analytic_point))
                    {
                        if (this->is_buyable_action(analytic_point))
                        {
                            return ACTIONABLE_BUYSELL_NEXT_SECOND;
                        }
                        else if (this->is_sellable_action(analytic_point))
                        {
                            return ACTIONABLE_SELLBUY_NEXT_SECOND;
                        }
                        else
                        {
                            return std::nullopt;
                        }
                    }
                    else
                    {
                        return std::nullopt;
                    }
                }
                else
                {
                    throw std::invalid_argument("bad actionable, bad prediction precision");   
                }
            }
    };
}

#endif