#define STRONG_MEMORY_ORDERING_FLAG true

#include "interval_coefficient_optimizer_tree.h"
#include <random>
#include <chrono>
#include <iostream>

void run_one_test()
{
    using namespace interval_coefficient_optimizer_tree;

    const size_t TREE_SZ_RANGE      = size_t{1} << 4;
    const size_t LEAF_SZ_RANGE      = size_t{1} << 4;
    const size_t TEST_SZ_RANGE      = size_t{1} << 4;
    const size_t BAD_NUMERIC_CHANCE = size_t{1} << 4;

    static auto randomizer          = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor    = std::uniform_int_distribution<size_t>{};
    static auto real_distributor    = std::uniform_real_distribution<double>{0, 1};

    size_t tree_sz                  = uint_distributor(randomizer) % TREE_SZ_RANGE + 1u;
    size_t leaf_sz                  = uint_distributor(randomizer) % LEAF_SZ_RANGE + 1u;
    size_t test_sz                  = uint_distributor(randomizer) % TEST_SZ_RANGE;

    CoefficientOptimizerTree tree(tree_sz, leaf_sz);

    for (size_t i = 0u; i < test_sz; ++i)
    {
        size_t first    = uint_distributor(randomizer) % tree_sz;
        size_t max_sz   = tree_sz - first;
        size_t sz       = uint_distributor(randomizer) % max_sz + 1u;

        auto tensor     = tree.get_coefficient_span({first, sz});

        if (tensor == nullptr)
        {
            std::cout << "mayday, null tensor" << std::endl;
            std::abort();
        }

        auto vec        = tensor->get_coefficient_space();

        stdx::xsafe_float_range_access(vec.data(), vec.size());

        size_t expected_sz  = sz;

        if (vec.size() != expected_sz)
        {
            std::cout << "mayday, unmatched size" << std::endl;
            std::abort();
        }

        double tentative_score = real_distributor(randomizer);

        if (uint_distributor(randomizer) % BAD_NUMERIC_CHANCE == 0u)
        {
            if (uint_distributor(randomizer) % 2 == 0u)
            {
                tentative_score = std::numeric_limits<double>::quiet_NaN();
            }
            else
            {
                tentative_score = std::numeric_limits<double>::infinity();
            }
        }

        tensor->feedback(tentative_score);
    }
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_FOCAL_TREE_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_FOCAL_TREE_TEST__" << std::endl;
}

int main()
{
    run_test();
}