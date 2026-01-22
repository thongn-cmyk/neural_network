#define STRONG_MEMORY_ORDERING_FLAG true

#include <iostream>
#include "temporal_coefficient_projector.h"
#include "temporal_coefficient_projector_2.h"
#include "temporal_coefficient_projector_3.h"
#include <random>
#include <chrono>
#include <functional>
#include <utility>
#include <algorithm>
#include <stdint.h>
#include <stdlib.h>
#include "conventional_randomizer.h"
#include "stdx.h"
#include "float_def.h"

using namespace float_def;

void test_projector(temporal_coefficient_projector::TemporalCoefficientProjectorInterface& projector,
                    size_t sz)
{
    static auto randomizer      = conventional_randomizer::ApplicationRandomizerObject();
    const size_t TEST_SZ_RANGE  = size_t{1} << 10;

    for (size_t i = 0u; i < TEST_SZ_RANGE; ++i)
    {
        std::vector<std_float_t> projected_coor = projector.project(randomizer.ld_randomize_focal(true));

        if (projected_coor.size() != sz)
        {
            std::cout << "mayday, bad projection size" << std::endl;
            std::abort();
        }

        try
        {
            stdx::xsafe_float_range_access(projected_coor.data(), projected_coor.size());
        }
        catch (...)
        {
            for (auto& e: projected_coor)
            {
                std::cout << e << "<>";
            }

            std::cout << std::endl;
            std::cout << "mayday, bad numeric access" << std::endl;

            std::abort();
        }
    }
}

void test_one_projector()
{
    const size_t COEFFICIENT_SZ_RANGE   = size_t{1} << 4;
    static auto randomizer              = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor        = std::uniform_int_distribution<size_t>();

    size_t coefficient_sz   = uint_distributor(randomizer) % COEFFICIENT_SZ_RANGE;
    auto projector          = temporal_coefficient_projector::CoefficientProjectorFactory::get_random_coefficient_projector(coefficient_sz);

    test_projector(*projector, coefficient_sz);
}

void test_two_projector()
{
    const size_t COEFFICIENT_SZ_RANGE   = size_t{1} << 4;
    static auto randomizer              = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor        = std::uniform_int_distribution<size_t>();

    size_t coefficient_sz   = uint_distributor(randomizer) % COEFFICIENT_SZ_RANGE;
    auto projector          = temporal_coefficient_projector::FixedOvalProjectorFactory<long double>{}.get_random_rotating_2_arm(coefficient_sz);

    test_projector(*projector, coefficient_sz);
}

void test_maybe_two_projector()
{
    const size_t COEFFICIENT_SZ_RANGE   = size_t{1} << 4;
    static auto randomizer              = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor        = std::uniform_int_distribution<size_t>();

    size_t coefficient_sz   = uint_distributor(randomizer) % COEFFICIENT_SZ_RANGE;

    auto generator          = [&](size_t sz)
    {
        const size_t TEST_SZ    = 3u;
        size_t test_idx         = uint_distributor(randomizer) % TEST_SZ;

        switch (test_idx)
        {
            case 0:
            {
                return temporal_coefficient_projector::FixedOvalProjectorFactory<long double>{}.get_random_oval_projector(sz);
            }
            case 1:
            {
                return temporal_coefficient_projector::FixedOvalProjectorFactory<long double>{}.get_random_rotating_2_arm(sz);
            }
            case 2:
            {
                return temporal_coefficient_projector::FixedOvalProjectorFactory<long double>{}.get_random_rotating_2_skewedarm(sz);
            }
            default:
            {
                std::unreachable();
            }
        }
    };

    auto projector          = temporal_coefficient_projector::RedistributedFocalFactory{}.get(generator, coefficient_sz);

    test_projector(*projector, coefficient_sz);
}

void test_tensor_projector()
{
    static auto factory                 = temporal_coefficient_projector_2::GeneratorFactory::get_best_generator();
    const size_t COEFFICIENT_SZ_RANGE   = size_t{1} << 4;
    const size_t BAD_NUMERIC_CHANCE     = size_t{1} << 4;
    static auto randomizer              = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor        = std::uniform_int_distribution<size_t>();
    static auto real_distributor        = std::uniform_real_distribution<double>();

    size_t coefficient_sz   = uint_distributor(randomizer) % COEFFICIENT_SZ_RANGE;
    auto tensor             = factory->get(coefficient_sz);
    auto projector          = tensor->get();
    double tentative_score  = real_distributor(randomizer);

    if (uint_distributor(randomizer) % BAD_NUMERIC_CHANCE == 0u)
    {
        if (uint_distributor(randomizer) % 2 == 0)
        {
            tentative_score = std::numeric_limits<double>::quiet_NaN();
        }
        else
        {
            tentative_score = std::numeric_limits<double>::infinity();
        }
    }

    test_projector(*projector, coefficient_sz);
    tensor->feedback(tentative_score);
}


void test_tensor_projector_2()
{
    const size_t COEFFICIENT_SZ_RANGE   = size_t{1} << 4;
    const size_t BAD_NUMERIC_CHANCE     = size_t{1} << 4;
    const size_t TEST_CHANCE            = size_t{1} << 16;
    static auto randomizer              = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor        = std::uniform_int_distribution<size_t>();
    static auto real_distributor        = std::uniform_real_distribution<double>();

    static auto factory                 = temporal_coefficient_projector_3::GeneratorFactory::get_normal_generator(8);
    static size_t coefficient_sz        = 8u;

    if (uint_distributor(randomizer) % TEST_CHANCE == 0u)
    {
        coefficient_sz = (uint_distributor(randomizer) % COEFFICIENT_SZ_RANGE) * 8u;
        factory = temporal_coefficient_projector_3::GeneratorFactory::get_normal_generator(coefficient_sz);
    }

    auto tensor             = factory->get();
    auto projector          = tensor->get();
    double tentative_score  = real_distributor(randomizer);

    if (uint_distributor(randomizer) % BAD_NUMERIC_CHANCE == 0u)
    {
        if (uint_distributor(randomizer) % 2 == 0)
        {
            tentative_score = std::numeric_limits<double>::quiet_NaN();
        }
        else
        {
            tentative_score = std::numeric_limits<double>::infinity();
        }
    }

    test_projector(*projector, coefficient_sz);
    tensor->feedback(tentative_score);
}

void test_tensor_projector_2_1()
{
    const size_t COEFFICIENT_SZ_RANGE   = size_t{1} << 4;
    const size_t BAD_NUMERIC_CHANCE     = size_t{1} << 4;
    const size_t TEST_CHANCE            = size_t{1} << 16;
    static auto randomizer              = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor        = std::uniform_int_distribution<size_t>();
    static auto real_distributor        = std::uniform_real_distribution<double>();

    static auto factory                 = temporal_coefficient_projector_3::GeneratorFactory::get_best_generator(8);
    static size_t coefficient_sz        = 8u;

    if (uint_distributor(randomizer) % TEST_CHANCE == 0u)
    {
        coefficient_sz = (uint_distributor(randomizer) % COEFFICIENT_SZ_RANGE) * 8u;
        factory = temporal_coefficient_projector_3::GeneratorFactory::get_best_generator(coefficient_sz);
    }

    auto tensor             = factory->get();
    auto projector          = tensor->get();
    double tentative_score  = real_distributor(randomizer);

    if (uint_distributor(randomizer) % BAD_NUMERIC_CHANCE == 0u)
    {
        if (uint_distributor(randomizer) % 2 == 0)
        {
            tentative_score = std::numeric_limits<double>::quiet_NaN();
        }
        else
        {
            tentative_score = std::numeric_limits<double>::infinity();
        }
    }

    test_projector(*projector, coefficient_sz);
    tensor->feedback(tentative_score);
}


void test_projector()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_PROJECTOR_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_projector();
        test_two_projector();
        test_maybe_two_projector();
        test_tensor_projector();
        test_tensor_projector_2();
        test_tensor_projector_2_1();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_PROJECTOR_TEST__" << std::endl;
}

int main()
{
    test_projector();
}