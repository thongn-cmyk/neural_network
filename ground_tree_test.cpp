#define STRONG_MEMORY_ORDERING_FLAG true

#include "ground_activator.h"
#include <chrono>
#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <functional>
#include <utility>
#include <algorithm>
#include <random>
#include "compact_serializer.h"

auto randomize_interval(size_t first, size_t last) -> std::pair<size_t, size_t>
{
    static auto randomizer  = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    size_t sz               = last - first;
    size_t offset           = randomizer() % sz;
    size_t new_first        = first + offset;
    size_t new_sz           = last - new_first;
    size_t range            = randomizer() % (new_sz + 1u);

    return {new_first, range};
}

auto randomize_tree_range() -> size_t
{
    const size_t RANGE_SZ   = size_t{1} << 14;
    static auto randomizer  = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    size_t tentative_sz     = randomizer() % RANGE_SZ;
    size_t actual_sz        = std::max(size_t{1}, tentative_sz);

    return actual_sz;
}

struct GroundTreeData
{
    size_t range;
    std::vector<std::pair<std::pair<size_t, size_t>, std::pair<size_t, size_t>>> interval_vec;
};

auto randomize_ground_tree_data() -> GroundTreeData
{
    const size_t INTERVAL_COUNT_SZ  = size_t{1} << 10;
    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    size_t interval_sz              = randomizer() % INTERVAL_COUNT_SZ;
    size_t tree_range               = randomize_tree_range();
    auto tree_data                  = GroundTreeData{};
    tree_data.range                 = tree_range;

    for (size_t i = 0u; i < interval_sz; ++i)
    {
        tree_data.interval_vec.push_back({randomize_interval(0u, tree_range), randomize_interval(0u, tree_range)});
    }

    return tree_data;
}

auto is_intersected(const std::pair<size_t, size_t>& lhs, const std::pair<size_t, size_t>& rhs) -> bool
{
    if (lhs.second == 0u)
    {
        return false;
    }

    if (rhs.second == 0u)
    {
        return false;
    }

    size_t lhs_first    = lhs.first;
    size_t lhs_last     = lhs.first + lhs.second;
    size_t rhs_first    = rhs.first;
    size_t rhs_last     = rhs.first + rhs.second;

    if (lhs_first >= rhs_last)
    {
        return false;
    }

    if (lhs_last <= rhs_first)
    {
        return false;
    }

    return true;
}

auto get_intervals_intersect_with(const GroundTreeData& data, const std::pair<size_t, size_t>& interval) -> std::vector<std::pair<size_t, size_t>>
{
    std::vector<std::pair<size_t, size_t>> result{};

    for (const auto& cmp_interval: data.interval_vec)
    {
        if (is_intersected(cmp_interval.first, interval))
        {
            result.push_back(cmp_interval.second);
        }
    }

    return result;
}

auto interval_sort_and_adjecent_join(const std::vector<std::pair<size_t, size_t>>& arg_vec) -> std::vector<std::pair<size_t, size_t>>
{
    auto less = [](const auto& lhs, const auto& rhs)
    {
        if (lhs.first < rhs.first)
        {
            return true;
        }

        if (lhs.first > rhs.first)
        {
            return false;
        }

        if (lhs.second < rhs.second)
        {
            return true;
        }

        if (lhs.second > rhs.second)
        {
            return false;
        }

        return false;
    };

    auto tmp_vec    = arg_vec;
    auto result_vec = std::vector<std::pair<size_t, size_t>>();

    std::sort(tmp_vec.begin(), tmp_vec.end(), less);

    std::optional<std::pair<size_t, size_t>> aggregated_interval = std::nullopt;

    for (size_t i = 0u; i < tmp_vec.size(); ++i)
    {
        if (!aggregated_interval.has_value())
        {
            aggregated_interval = tmp_vec[i];
            continue;
        }

        size_t now_last     = aggregated_interval->first + aggregated_interval->second;
        size_t nxt_first    = tmp_vec[i].first;
        size_t nxt_last     = tmp_vec[i].first + tmp_vec[i].second;

        if (now_last >= nxt_first)
        {
            if (nxt_last > now_last)
            {
                size_t new_sz = nxt_last - aggregated_interval->first;
                aggregated_interval->second = new_sz;
            }

            continue;
        }

        result_vec.push_back(aggregated_interval.value());
        aggregated_interval = tmp_vec[i];
    }

    if (aggregated_interval.has_value())
    {
        result_vec.push_back(aggregated_interval.value());
    }

    return result_vec;
}

auto zero_trim(const std::vector<std::pair<size_t, size_t>>& arg_vec) -> std::vector<std::pair<size_t, size_t>>
{
    auto result_vec = std::vector<std::pair<size_t, size_t>>();

    for (const auto& [first, sz]: arg_vec)
    {
        if (sz != 0u)
        {
            result_vec.push_back({first, sz});
        }
    }

    return result_vec;
}

auto print(const std::vector<std::pair<size_t, size_t>>& interval_vec)
{
    for (const auto& interval: interval_vec)
    {
        std::cout << interval.first << " "  << interval.second << std::endl;
    }
}

auto to_unique_representation(const std::vector<std::pair<size_t, size_t>>& interval_vec) -> std::string
{
    // auto tmp    = interval_vec;
    // std::sort(tmp.begin(), tmp.end());

    return dg::network_compact_serializer::serialize<std::string>(interval_sort_and_adjecent_join(interval_vec));
}

void test_ground_tree()
{
    using namespace ground_activator;

    size_t TEST_SZ                  = size_t{1} << 14;
    size_t INTERVAL_TEST_SZ_RANGE   = size_t{1} << 8;
    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        GroundTreeData tree_data    = randomize_ground_tree_data();
        GroundTree tree(tree_data.range);
        size_t interval_test_sz     = randomizer() % INTERVAL_TEST_SZ_RANGE;

        for (const auto& interval_pair: tree_data.interval_vec)
        {
            tree.add_ground(interval_pair.first, interval_pair.second);
        }

        if (randomizer() % 2 == 0)
        {
            tree.optimize();
        }

        for (size_t j = 0u; j < interval_test_sz; ++j)
        {
            auto random_interval        = randomize_interval(0u, tree_data.range);

            auto associated_intervals   = zero_trim(get_intervals_intersect_with(tree_data, random_interval));
            auto tree_intervals         = tree.get_ground(random_interval);

            if (to_unique_representation(associated_intervals) != to_unique_representation(tree_intervals))
            {
                // std::cout << to_unique_representation(associated_intervals) << std::endl;
                // std::cout << to_unique_representation(tree_intervals) << std::endl;
                print(interval_sort_and_adjecent_join(associated_intervals));
                std::cout << "------------" << std::endl;
                print(interval_sort_and_adjecent_join(tree_intervals));
                std::cout << "------------" << std::endl;

                std::cout << "mayday" << std::endl;
                std::abort();
            }
        }

        std::cout << "pass > " << i << "/" << TEST_SZ << std::endl;
    }
}

int main()
{
    test_ground_tree();
}