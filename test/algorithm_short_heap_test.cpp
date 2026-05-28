#include <iostream>
#include <algorithm_extension/short_heap.h>
#include <algorithm>
#include <random>
#include <utility>
#include <functional>
#include <cstdlib>
#include <chrono>

auto randomize_int(size_t range) -> size_t
{
    if (range == 0u)
    {
        std::abort();
    }

    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return randomizer() % range;
}

auto randomize_int_vector(size_t range, size_t value_range = 16) -> std::vector<size_t>
{
    std::vector<size_t> rs{};

    for (size_t i = 0u; i < range; ++i)
    {
        rs.push_back(randomize_int(value_range));
    }

    return rs;
}

void test_is_heap()
{
    std::vector<size_t> random_arr      = randomize_int_vector(randomize_int(size_t{1} << 3));
    std::vector<size_t> random_arr_1    = random_arr;

    if (std::is_heap(random_arr.begin(), random_arr.end()) != algorithm_extension::is_heap(random_arr.begin(), random_arr.end(), algorithm_extension::GreaterEqualCmp{}, std::integral_constant<size_t, 2>{}))
    {
        std::cout << "mayday, bad is heap test, mismatched result\n";
        std::abort();
    }
}

void test_sort()
{
    std::vector<size_t> random_arr      = randomize_int_vector(randomize_int(size_t{1} << 4));
    std::vector<size_t> random_arr_1    = random_arr;

    std::sort(random_arr.begin(), random_arr.end(), std::greater<size_t>{});

    algorithm_extension::make_heap(random_arr_1.begin(), random_arr_1.end());
    algorithm_extension::sort_heap(random_arr_1.begin(), random_arr_1.end());

    if (random_arr != random_arr_1)
    {
        std::cout << "mayday, bad sort test, mismatched sorting sequence\n";

        for (size_t c: random_arr_1)
        {
            std::cout << c << "\n";
        }

        std::abort();
    }
}

void test_top_k()
{
    std::vector<size_t> random_arr      = randomize_int_vector(randomize_int(size_t{1} << 4));
    std::vector<size_t> random_arr_1    = random_arr;
    size_t top_k                        = randomize_int(random_arr.size() + 1u);

    std::sort(random_arr.begin(), random_arr.end(), std::less<size_t>{});

    algorithm_extension::make_heap(random_arr_1.begin(), random_arr_1.end());
    auto first                          = algorithm_extension::top_k(random_arr_1.begin(), random_arr_1.end(), top_k);

    std::vector<size_t> rev_top_k_arr   = std::vector<size_t>(first, random_arr_1.end());
    std::vector<size_t> top_k_arr       = std::vector<size_t>(rev_top_k_arr.rbegin(), rev_top_k_arr.rend());
    std::vector<size_t> expected        = std::vector<size_t>(random_arr.begin(), std::next(random_arr.begin(), top_k));

    if (top_k_arr != expected)
    {
        std::cout << "mayday, bad top k, mismatched sorting sequence\n";
        std::abort();
    }
}

void test_push_pop()
{
    const size_t TEST_SZ    = size_t{1} << 6;

    std::vector<size_t> lhs{};
    std::vector<size_t> rhs{};

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        if (randomize_int(2) == 0)
        {
            size_t random_value = randomize_int(size_t{1} << 4);

            lhs.push_back(random_value);
            rhs.push_back(random_value);

            std::push_heap(lhs.begin(), lhs.end());
            algorithm_extension::push_heap(rhs.begin(), rhs.end());
        }
        else
        {
            if (lhs.size() == 0u)
            {
                continue;
            }

            std::pop_heap(lhs.begin(), lhs.end());
            algorithm_extension::pop_heap(rhs.begin(), rhs.end());

            lhs.pop_back();
            rhs.pop_back();
        }
    }
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_ALGORITHM_SHORT_HEAP_TEST__\n";

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_is_heap();
        test_sort();
        test_push_pop();
        test_top_k();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    std::cout << "__END_ALGORITHM_SHORT_HEAP_TEST__\n";
}

int main()
{
    run_test();    
}