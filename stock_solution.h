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

namespace stock_solution
{
    //let's see if we can make it 1 million bucks/ second this week, stay very tuned
    //are you IN THE MONEY or OUT THE MONEY?
    //we'd attempt to do a 10-100 million extraction, then that's it. On the candlesticks!
    //we only need to be confident once in a while, that's how we do that, baby!

    struct SolutionData
    {

    };

    class SolutionSerializer
    {

    };

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
            static inline constexpr uint8_t FEATURIZATION_SECOND_ORDER_SUFFIX           = 2u;
            static inline constexpr uint8_t FEATURIZATION_FIRST_ORDER_SUFFIX            = 3u;

            static inline constexpr size_t MAX_DISCRETIZATION_SZ                        = 16u;

        private:

            struct FeatureTimePoint
            {
                double epoch_timepoint;
                double feature_value;
            };

            uint8_t focal_unit;
            double focal_base;
            size_t focal_sz;
            size_t focal_discretization_sz;
            uint8_t featurization_option;

            std::vector<TickerData> ticker_data_vec;
            std::vector<std::string> feature_name_vec;
            std::unordered_map<std::string, std::unordered_map<std::string, std::vector<FeatureTimePoint>>> feature_map;

        public:

            TemporalFeatureExtractor(): focal_unit(FOCAL_UNIT_MINUTE),
                                        focal_base(10),
                                        focal_sz(8),
                                        focal_discretization_sz(8),
                                        featurization_option(FEATURIZATION_FIRST_ORDER_BINARY_SUFFIX),
                                        ticker_data_vec(),
                                        feature_name_vec(),
                                        feature_map(){}

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
                    case FEATURIZATION_SECOND_ORDER_SUFFIX: 
                    case FEATURIZATION_FIRST_ORDER_SUFFIX:
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

            auto compute() -> TemporalFeatureExtractor&
            {
                this->feature_map.clear();

                std::unordered_map<std::string, std::unordered_map<std::string, std::map<double, double>>> intermediate_map{};

                for (const TickerData& ticker_data: this->ticker_data_vec)
                {
                    double lapsed_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(ticker_data.timestamp.time_since_epoch()).count();
                    intermediate_map[ticker_data.ticker_name][ticker_data.feature_name].insert({lapsed_since_epoch, ticker_data.feature_value});
                }

                for (const auto& [ticker_name, ticker_data]: intermediate_map)
                {
                    for (const auto& [feature_name, feature_data]: ticker_data)
                    {
                        for (const auto& [epoch_timestamp, feature_value]: feature_data)
                        {
                            this->feature_map[ticker_name][feature_name].push_back(FeatureTimePoint{.epoch_timepoint    = epoch_timestamp,
                                                                                                    .feature_value      = feature_value});
                        }
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
                    case FEATURIZATION_SECOND_ORDER_SUFFIX:
                    {
                        return this->second_order_featurize(raw_feature_map);
                    }
                    case FEATURIZATION_FIRST_ORDER_SUFFIX:
                    {
                        return this->first_order_featurize(raw_feature_map);
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

        private:

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

            auto get_timeslice_vector_since(std::chrono::time_point<std::chrono::system_clock> timepoint) -> std::vector<std::vector<std::pair<double, double>>>
            {
                std::vector<std::vector<std::pair<double, double>>> result{};
                std::chrono::nanoseconds unit_dur = this->get_unit_duration();

                for (size_t i = 0u; i < this->focal_sz; ++i)
                {
                    std::vector<std::pair<double, double>> window_vec   = {};
                    double focal_value                                  = std::pow(this->focal_base, i);

                    auto first                                          = timepoint - (unit_dur * focal_value);
                    auto last                                           = timepoint;

                    uint64_t first_epoch                                = std::chrono::duration_cast<std::chrono::nanoseconds>(first.time_since_epoch()).count();
                    uint64_t last_epoch                                 = std::chrono::duration_cast<std::chrono::nanoseconds>(last.time_since_epoch()).count();
                    uint64_t lapsed                                     = last_epoch - first_epoch;
                    uint64_t interval_sz                                = lapsed / this->focal_discretization_sz;

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

            auto binary_seek_first_helper(FeatureTimePoint * first, FeatureTimePoint * last, double epoch_timepoint) -> FeatureTimePoint *
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

            auto binary_seek_first(FeatureTimePoint * first, FeatureTimePoint * last, double epoch_timepoint) -> FeatureTimePoint *
            {
                intmax_t chk_sz = std::distance(first, last);

                if (chk_sz < 0)
                {
                    std::abort();
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

                if (sz <= 2u)
                {
                    std::abort();
                }

                return this->binary_seek_first_helper(first, last, epoch_timepoint);
            }

            //front() <= x < back()
            //return back()

            auto binary_seek_last_helper(FeatureTimePoint * first, FeatureTimePoint * last, double epoch_timepoint) -> FeatureTimePoint *
            {
                size_t sz = std::distance(first, last);

                if (sz == 2u)
                {
                    return std::next(first);
                }

                size_t mid_sz                   = sz / 2;
                FeatureTimePoint * nxt_point    = std::next(first, mid_sz);

                if (epoch_timepoint >= nxt_point->epoch_timepoint)
                {
                    return this->binary_seek_last_helper(nxt_point, last, epoch_timepoint);
                }

                return this->binary_seek_last_helper(first, std::next(nxt_point), epoch_timepoint);
            }

            auto binary_seek_last(FeatureTimePoint * first, FeatureTimePoint * last, double epoch_timepoint) -> FeatureTimePoint *
            {
                intmax_t chk_sz = std::distance(first, last);

                if (chk_sz < 0)
                {
                    std::abort();
                }

                size_t sz       = chk_sz;

                if (sz == 0u)
                {
                    return last;
                }

                if (first->epoch_timepoint > epoch_timepoint)
                {
                    return first;                    
                }

                if (epoch_timepoint >= std::prev(last)->epoch_timepoint)
                {
                    return last;
                }

                if (sz <= 2u)
                {
                    std::abort();
                }

                return this->binary_seek_last_helper(first, last, epoch_timepoint);
            }

            auto binary_interval(FeatureTimePoint * first,
                                 FeatureTimePoint * last,
                                 double epoch_timepoint_first,
                                 double epoch_timepoint_last) -> std::pair<std::add_pointer_t<FeatureTimePoint>, std::add_pointer_t<FeatureTimePoint>>
            {
                FeatureTimePoint * finding_first    = this->binary_seek_first(first, last, epoch_timepoint_first);
                FeatureTimePoint * finding_last     = this->binary_seek_last(first, last, epoch_timepoint_last);
                intmax_t tentative_sz               = std::distance(finding_first, finding_last);
                intmax_t sz                         = std::max(intmax_t{0}, tentative_sz);

                return std::make_pair(finding_first, std::next(finding_first, sz));
            }

            auto get_best_feature_for_window(const std::string& ticker_name,
                                             const std::string& feature_name,
                                             double epoch_timepoint_first,
                                             double epoch_timepoint_last) -> std::optional<double>
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

                FeatureTimePoint * first            = map_ptr_2->second.data();
                FeatureTimePoint * last             = std::next(first, map_ptr_2->second.size());
                auto [finding_first, finding_last]  = this->binary_interval(first, last, epoch_timepoint_first, epoch_timepoint_last);
                auto max_ptr                        = std::min_element(finding_first, finding_last, [](const auto& lhs, const auto& rhs){return lhs.feature_value < rhs.feature_value;});

                if (max_ptr == finding_last)
                {
                    return std::nullopt;
                }

                return max_ptr->feature_value;
            }

            auto get_raw_feature_vector_at_timepoint(const std::string& ticker_name,
                                                     std::chrono::time_point<std::chrono::system_clock> timepoint) -> std::unordered_map<std::string, std::vector<std::vector<std::optional<double>>>>
            {
                std::vector<std::vector<std::pair<double, double>>> timeslice_2d_vec                = this->get_timeslice_vector_since(timepoint);
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

            auto first_order_featurize(const std::unordered_map<std::string, std::vector<std::vector<std::optional<double>>>>& raw_map) -> std::vector<double>
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
                            feature_vec.push_back(Base10ExponentialRadixer{}.enumerate(nxt_vec.front()));
                        }

                        std::copy(suffix_vec.begin(), suffix_vec.end(), std::back_inserter(feature_vec));
                    }
                }

                return feature_vec;
            }

            auto second_order_featurize(const std::unordered_map<std::string, std::vector<std::vector<std::optional<double>>>>& raw_map) -> std::vector<double>
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
                            feature_vec.push_back(Base10ExponentialRadixer{}.enumerate(nxt_vec.front()));   
                        }

                        std::copy(suffix_vec.begin(), suffix_vec.end(), std::back_inserter(feature_vec));

                        if (!nxt_vec.empty())
                        {
                            auto [suffix_vec_2, nxt_vec_2]          = SequenceCompressor{}.suffix_lossless_compress({std::next(nxt_vec.begin()), nxt_vec.end()});

                            if (!nxt_vec_2.empty())
                            {
                                feature_vec.push_back(Base10ExponentialRadixer{}.enumerate(nxt_vec_2.front()));
                            }

                            std::copy(suffix_vec_2.begin(), suffix_vec_2.end(), std::back_inserter(feature_vec));
                        }
                    }
                }

                return feature_vec;
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
                            size_t enumeration_idx      = Base10ExponentialRadixer{}.enumerate(nxt_vec.front());
                            size_t enumeration_sz       = Base10ExponentialRadixer{}.enumeration_size();
                            std::vector<double> tmp_vec = this->hole_punch(enumeration_idx, enumeration_sz);

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
                            size_t enumeration_idx      = Base10ExponentialRadixer{}.enumerate(nxt_vec.front());
                            size_t enumeration_sz       = Base10ExponentialRadixer{}.enumeration_size();
                            std::vector<double> tmp_vec = this->hole_punch(enumeration_idx, enumeration_sz);

                            std::copy(tmp_vec.begin(), tmp_vec.end(), std::back_inserter(feature_vec));
                        }

                        for (size_t suffix: suffix_vec)
                        {
                            std::vector<double> tmp_vec = this->hole_punch(suffix, suffix_vec.size());
                            std::copy(tmp_vec.begin(), tmp_vec.end(), std::back_inserter(feature_vec));
                        }

                        if (!nxt_vec.empty())
                        {
                            auto [suffix_vec_2, nxt_vec_2]  = SequenceCompressor{}.suffix_lossless_compress({std::next(nxt_vec.begin()), nxt_vec.end()});

                            if (!nxt_vec_2.empty())
                            {
                                size_t enumeration_idx      = Base10ExponentialRadixer{}.enumerate(nxt_vec_2.front());
                                size_t enumeration_sz       = Base10ExponentialRadixer{}.enumeration_size();
                                std::vector<double> tmp_vec = this->hole_punch(enumeration_idx, enumeration_sz);

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
    };

    class SolutionTrainer
    {
        public:

            static inline constexpr uint8_t LOW_COMPUTE     = 0u;
            static inline constexpr uint8_t MID_COMPUTE     = 1u;
            static inline constexpr uint8_t HIGH_COMPUTE    = 2u;

            static inline constexpr uint8_t LOW_STORAGE     = 0u;
            static inline constexpr uint8_t MID_STORAGE     = 1u;
            static inline constexpr uint8_t HIGH_STORAGE    = 2u;

            auto set_compute(uint8_t option) -> SolutionTrainer&
            {
                return *this;
            }

            auto set_storage(uint8_t option) -> SolutionTrainer&
            {
                return *this;
            }

            auto set_data(const std::vector<TickerData>& data) -> SolutionTrainer&
            {
                return *this;
            }

            auto set_training_first_timepoint(std::chrono::time_point<std::chrono::system_clock> timepoint) -> SolutionTrainer&
            {
                return *this;
            }

            auto set_training_last_timepoint(std::chrono::time_point<std::chrono::system_clock> timepoint) -> SolutionTrainer&
            {
                return *this;
            }

            auto set_tickers(const std::vector<std::string>& ticker_vec) -> SolutionTrainer&
            {
                return *this;
            }

            auto set_training_intput_output_lapsed(std::chrono::nanoseconds dur) -> SolutionTrainer&
            {
                return *this;
            }

            auto train() -> SolutionData
            {
                return {};
            }
    };

    static inline const std::string ACTIONABLE_BUYSELL_NEXT_SECOND      = "buysell_next_second";
    static inline const std::string ACTIONABLE_BUYSELL_NEXT_MILLISECOND = "buysell_next_millisecond";
    static inline const std::string ACTIONABLE_SELLBUY_NEXT_SECOND      = "sellbuy_next_second";
    static inline const std::string ACTIONABLE_SELLBUY_NEXT_MILLISECOND = "sellbuy_next_millisecond";

    class Actionable
    {
        std::string ticker_name;
        std::string actionable_type;
        double confident_score;
    };

    class SolutionProduct
    {
        private:

            SolutionData data;

        public:

            SolutionProduct(SolutionData data): data(std::move(data)){}

            auto load_data(const std::vector<TickerData>& ticker_data) -> SolutionProduct&
            {
                return *this;
            }

            auto predict(std::chrono::time_point<std::chrono::system_clock> forecast_timepoint) -> std::vector<Actionable>
            {
                return {};
            }
    };
}

#endif