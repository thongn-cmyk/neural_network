#define STRONG_MEMORY_ORDERING_FLAG true

#include <iostream>
#include "stock_solution.h"
#include <stdint.h>
#include <stdlib.h>
#include <random>
#include <functional>
#include <chrono>
#include "conventional_randomizer.h"
#include <unordered_map>
#include <unordered_set>

using namespace stock_solution;

auto randomize_focal_unit() -> uint8_t
{
    static auto randomizer      = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    const size_t ENUMERATION_SZ = 5u;
    size_t enumeration_idx      = randomizer() % ENUMERATION_SZ;

    switch (enumeration_idx)
    {
        case 0:
        {
            return TemporalFeatureExtractor::FOCAL_UNIT_MICROSECOND;
        }
        case 1:
        {
            return TemporalFeatureExtractor::FOCAL_UNIT_MILLISECOND;
        }
        case 2:
        {
            return TemporalFeatureExtractor::FOCAL_UNIT_SECOND;
        }
        case 3:
        {
            return TemporalFeatureExtractor::FOCAL_UNIT_MINUTE;
        }
        case 4:
        {
            return TemporalFeatureExtractor::FOCAL_UNIT_DAY;
        }
        default:
        {
            std::abort();
        }
    }
}

auto randomize_featurization_option() -> uint8_t
{
    static auto randomizer      = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    const size_t ENUMERATION_SZ = 4u;
    size_t enumeration_idx      = randomizer() % ENUMERATION_SZ;

    switch (enumeration_idx)
    {
        case 0:
        {
            return TemporalFeatureExtractor::FEATURIZATION_SECOND_ORDER_BINARY_SUFFIX;
        }
        case 1:
        {
            return TemporalFeatureExtractor::FEATURIZATION_FIRST_ORDER_BINARY_SUFFIX;
        }
        default:
        {
            std::abort();
        }
    }
}

auto randomize_exponential_base() -> double
{
    static auto randomizer      = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    const double BASE_FIRST     = 0.0001;
    const double BASE_LAST      = 99.9999;
    static auto real_dis        = std::uniform_real_distribution<double>(BASE_FIRST, BASE_LAST);

    return real_dis(randomizer);
}

auto randomize_focal_step() -> size_t
{
    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    const size_t FOCAL_STEP_RANGE   = 10u;

    return randomizer() % FOCAL_STEP_RANGE;
}

auto randomize_discretization_step() -> size_t
{
    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    const size_t FOCAL_STEP_RANGE   = 10u;

    return randomizer() % FOCAL_STEP_RANGE + 1u;
}

auto randomize_timepoint() -> std::chrono::time_point<std::chrono::system_clock>
{
    using operating_float_t         = long double;

    static auto focal_randomizer    = conventional_randomizer::ApplicationRandomizerObject{};
    uint64_t now_tick               = std::chrono::system_clock::now().time_since_epoch().count();
    operating_float_t perc          = focal_randomizer.ld_randomize_focal_2() * focal_randomizer.ld_randomize_focal_2();
    uint64_t tick                   = std::clamp(static_cast<uint64_t>(now_tick * perc), uint64_t{0u}, now_tick);
    uint64_t new_tick               = now_tick - tick;

    return std::chrono::time_point<std::chrono::system_clock>(typename decltype(std::chrono::system_clock::now())::duration(new_tick));
}

auto randomize_string() -> std::string
{
    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    static auto char_randomizer     = std::bind(std::uniform_int_distribution<uint8_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    size_t STRING_SZ_RANGE          = size_t{1} << 4;

    size_t string_sz                = randomizer() % STRING_SZ_RANGE;

    std::vector<char> rs(string_sz);
    std::generate(rs.begin(), rs.end(), std::ref(char_randomizer));

    return std::string(rs.begin(), rs.end());
}

auto randomize_string_vec(size_t sz) -> std::vector<std::string>
{
    std::vector<std::string> result_vec{};

    for (size_t i = 0u; i < sz; ++i)
    {
        result_vec.push_back(randomize_string());
    }

    return result_vec;
}

auto randomize_ticker_vec() -> std::vector<std::string>
{
    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    const size_t TICKER_SZ_RANGE    = size_t{1} << 4;

    return randomize_string_vec(randomizer() % TICKER_SZ_RANGE + 1u);
}

auto randomize_feature_vec() -> std::vector<std::string>
{
    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    const size_t FEATURE_SZ_RANGE   = size_t{1} << 4;

    return randomize_string_vec(randomizer() % FEATURE_SZ_RANGE + 1u);
}

auto randomize_ticker_data() -> std::vector<TickerData>
{
    static auto randomizer                  = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    const double BASE_FIRST                 = 0.0001;
    const double BASE_LAST                  = 99.9999;
    const size_t TICKER_SZ_RANGE            = size_t{1} << 4;

    static auto uint_dis                    = std::uniform_int_distribution<size_t>{};
    static auto real_dis                    = std::uniform_real_distribution<double>{};

    size_t ticker_sz                        = uint_dis(randomizer) % TICKER_SZ_RANGE;
    std::vector<TickerData> result          = std::vector<TickerData>{};
    std::vector<std::string> ticker_vec     = randomize_ticker_vec();
    std::vector<std::string> feature_vec    = randomize_feature_vec();

    for (size_t i = 0u; i < ticker_sz; ++i)
    {
        result.push_back(TickerData
        {
            .ticker_name    = ticker_vec[uint_dis(randomizer) % ticker_vec.size()],
            .feature_name   = feature_vec[uint_dis(randomizer) % feature_vec.size()],
            .feature_value  = real_dis(randomizer),
            .timestamp      = randomize_timepoint()
        });
    }

    return result;
}

auto get_random_feature_name_list(const std::vector<TickerData>& ticker_data_vec) -> std::vector<std::string>
{
    std::unordered_set<std::string> feature_name_set{};

    for (const TickerData& ticker_data: ticker_data_vec)
    {
        feature_name_set.insert(ticker_data.feature_name);
    }

    std::vector<std::string> feature_name_vec(feature_name_set.begin(), feature_name_set.end());

    static auto randomizer  = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_dis    = std::uniform_int_distribution<size_t>{};

    std::shuffle(feature_name_vec.begin(), feature_name_vec.end(), randomizer);
    size_t resize_sz;

    if (feature_name_vec.empty())
    {
        resize_sz = 0u;
    }
    else
    {
        resize_sz = uint_dis(randomizer) % feature_name_vec.size();
    }

    feature_name_vec.resize(resize_sz);

    return feature_name_vec;
}

auto randomize_ticker_name_list(const std::vector<TickerData>& ticker_data_vec) -> std::vector<std::string>
{
    std::unordered_set<std::string> ticker_name_set{};

    for (const TickerData& ticker_data: ticker_data_vec)
    {
        ticker_name_set.insert(ticker_data.ticker_name);
    }

    std::vector<std::string> ticker_name_vec(ticker_name_set.begin(), ticker_name_set.end());

    static auto randomizer  = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_dis    = std::uniform_int_distribution<size_t>{};

    std::shuffle(ticker_name_vec.begin(), ticker_name_vec.end(), randomizer);
    size_t resize_sz;

    if (ticker_name_vec.empty())
    {
        resize_sz = 0u;
    }
    else
    {
        resize_sz = uint_dis(randomizer) % ticker_name_vec.size();
    }

    ticker_name_vec.resize(resize_sz);

    return ticker_name_vec;
}

void test_feature_vector(const std::vector<double>& feature_vec)
{
    try
    {
        stdx::safe_float_range_access(feature_vec.data(), feature_vec.size());
    }
    catch (...)
    {
        std::cout << "mayday, bad feature numeric value" << std::endl;
        std::abort();
    }
}

void test_one_featurization()
{
    TemporalFeatureExtractor extractor{};
    std::vector<TickerData> ticker_data = randomize_ticker_data();

    extractor.set_focal_unit(randomize_focal_unit())
             .set_focal_exponential_base(randomize_exponential_base())
             .set_focal_step(randomize_focal_step())
             .set_featurization_option(randomize_featurization_option())
             .set_focal_discretization_size(randomize_discretization_step())
             .set_data(ticker_data)
             .set_feature_name_list(get_random_feature_name_list(ticker_data))
             .compute();

    std::vector<std::string> ticker_name_list = randomize_ticker_name_list(ticker_data);

    for (const std::string& ticker_name: ticker_name_list)
    {
        auto timepoint  = randomize_timepoint();
        auto feat_vec   = extractor.get_feature_vector_at_timepoint(ticker_name, timepoint);

        test_feature_vector(feat_vec);
    }
}

void test_featurization()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_FEATURIZATION_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_featurization();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_FEATURIZATION_TEST__" << std::endl;
}

int main()
{
    test_featurization();    
}