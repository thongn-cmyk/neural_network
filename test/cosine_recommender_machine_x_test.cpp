#define STRONG_MEMORY_ORDERING_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <matrix_steering_subsystem/cosine_recommender_machine_x.h>
#include <random>
#include <functional>
#include <stl_extension/stdx.h>

void run_one_test()
{
    using namespace cosine_recommender_machine_x;

    const size_t SPACE_SZ_RANGE         = size_t{1} << 4;
    const size_t TEST_SZ_RANGE          = size_t{1} << 8;
    const size_t WRONG_NUMERIC_CHANCE   = size_t{1} << 4;

    static auto seed_randomizer         = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor        = std::uniform_int_distribution<size_t>();
    static auto real_distributor        = std::uniform_real_distribution<double>(0, 1);

    size_t space_sz                     = uint_distributor(seed_randomizer) % SPACE_SZ_RANGE;
    size_t test_sz                      = uint_distributor(seed_randomizer) % TEST_SZ_RANGE;

    std::unique_ptr<CosineRecommenderMachineInterface> machine = MachineFactory::get_trad_stat_recommender_machine(space_sz);

    for (size_t i = 0u; i < test_sz; ++i)
    {
        std::unique_ptr<CosineRecommendationResultInterface> result = machine->next();

        if (result == nullptr)
        {
            std::cout << "mayday, result is null" << std::endl;
            std::abort();
        }

        std::vector<crm_x_float_t> vec = result->get();
        stdx::xsafe_float_range_access(vec.data(), vec.size());

        double tentative_result = real_distributor(seed_randomizer);

        if (uint_distributor(seed_randomizer) % WRONG_NUMERIC_CHANCE == 0u)
        {
            if (uint_distributor(seed_randomizer) % 2 == 0u)
            {
                tentative_result = std::numeric_limits<double>::quiet_NaN();
            }
            else
            {
                tentative_result = std::numeric_limits<double>::infinity();
            }
        }

        result->feedback(tentative_result);
    }
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_COSINE_RECOMMENDER_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_COSINE_RECOMMENDER_TEST__" << std::endl;
}

int main()
{
    run_test();    
}