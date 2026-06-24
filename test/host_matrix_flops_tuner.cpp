#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <taylor_matrix/host_matrix/shape_projection.h>
#include <chrono>
#include <functional>
#include <random>
#include <algorithm>
#include <utility>
#include <stl_extension/stdx.h>
#include <iostream>

auto randomize_float(double first, double last) -> float
{
    static auto randomizer  = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto distributor = std::uniform_real_distribution<float>(first, last);

    return distributor(randomizer);
}

auto randomize_float_vec(size_t sz) -> std::vector<float>
{
    constexpr float FIRST   = -1;
    constexpr float LAST    = 1;

    std::vector<float> rs   = {};

    for (size_t i = 0u; i < sz; ++i)
    {
        rs.push_back(randomize_float(FIRST, LAST));
    }

    return rs;
}

void test_batch_8()
{
    constexpr size_t BATCH_SZ                   = 8u;
    constexpr size_t BASE_SZ                    = 4u;
    constexpr size_t LOOP_SZ                    = size_t{1} << 30;
    constexpr size_t OPERATION_PER_COEFFICIENT  = 4u;

    std::vector<float> inp_vec                  = randomize_float_vec(BATCH_SZ);
    std::vector<float> coeff_vec                = randomize_float_vec(BASE_SZ);
    std::vector<float> out_vec                  = std::vector<float>(inp_vec.size(), 0);

    double total                    = 0;

    auto clock_first                = std::chrono::high_resolution_clock::now();

    for (size_t i = 0u; i < LOOP_SZ; ++i)
    {
        taylor_matrix::host_matrix::shape_projection::base_batch_taylor_shape_project(inp_vec.data(), stdx::to_size_container(std::integral_constant<size_t, BATCH_SZ>{}),
                                                                                      coeff_vec.data(), stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>{}),
                                                                                      out_vec.data());

        for (size_t j = 0u; j < BATCH_SZ; ++j)
        {
            total += out_vec[j];
        }
    }

    auto clock_last                 = std::chrono::high_resolution_clock::now();
    intmax_t lapsed                 = std::chrono::duration_cast<std::chrono::milliseconds>(clock_last - clock_first).count();
    intmax_t nano_lapsed            = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_last - clock_first).count();
    size_t total_flops              = LOOP_SZ * BATCH_SZ * BASE_SZ * OPERATION_PER_COEFFICIENT;
    double ops_per_tick             = static_cast<double>(total_flops) / nano_lapsed;

    if (!std::isnan(total))
    {
        std::cout << "flops > " << total_flops << "<> lapsed > " << lapsed << "<ms>" << ops_per_tick << "<ops_per_tick>\n";
    }
}

void test_batch_16()
{
    constexpr size_t BATCH_SZ                   = 16u;
    constexpr size_t BASE_SZ                    = 4u;
    constexpr size_t LOOP_SZ                    = size_t{1} << 30;
    constexpr size_t OPERATION_PER_COEFFICIENT  = 4u;

    std::vector<float> inp_vec                  = randomize_float_vec(BATCH_SZ);
    std::vector<float> coeff_vec                = randomize_float_vec(BASE_SZ);
    std::vector<float> out_vec                  = std::vector<float>(inp_vec.size(), 0);

    double total                    = 0;

    auto clock_first                = std::chrono::high_resolution_clock::now();

    for (size_t i = 0u; i < LOOP_SZ; ++i)
    {
        taylor_matrix::host_matrix::shape_projection::base_batch_taylor_shape_project(inp_vec.data(), stdx::to_size_container(std::integral_constant<size_t, BATCH_SZ>{}),
                                                                                      coeff_vec.data(), stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>{}),
                                                                                      out_vec.data());

        for (size_t j = 0u; j < BATCH_SZ; ++j)
        {
            total += out_vec[j];
        }
    }

    auto clock_last                 = std::chrono::high_resolution_clock::now();
    intmax_t lapsed                 = std::chrono::duration_cast<std::chrono::milliseconds>(clock_last - clock_first).count();
    intmax_t nano_lapsed            = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_last - clock_first).count();
    size_t total_flops              = LOOP_SZ * BATCH_SZ * BASE_SZ * OPERATION_PER_COEFFICIENT;
    double ops_per_tick             = static_cast<double>(total_flops) / nano_lapsed;

    if (!std::isnan(total))
    {
        std::cout << "flops > " << total_flops << "<> lapsed > " << lapsed << "<ms>" << ops_per_tick << "<ops_per_tick>\n";
    }
}

void test_batch_32()
{
    constexpr size_t BATCH_SZ                   = 32u;
    constexpr size_t BASE_SZ                    = 4u;
    constexpr size_t LOOP_SZ                    = size_t{1} << 30;
    constexpr size_t OPERATION_PER_COEFFICIENT  = 4u;

    std::vector<float> inp_vec                  = randomize_float_vec(BATCH_SZ);
    std::vector<float> coeff_vec                = randomize_float_vec(BASE_SZ);
    std::vector<float> out_vec                  = std::vector<float>(inp_vec.size(), 0);

    double total                    = 0;

    auto clock_first                = std::chrono::high_resolution_clock::now();

    for (size_t i = 0u; i < LOOP_SZ; ++i)
    {
        taylor_matrix::host_matrix::shape_projection::base_batch_taylor_shape_project(inp_vec.data(), stdx::to_size_container(std::integral_constant<size_t, BATCH_SZ>{}),
                                                                                      coeff_vec.data(), stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>{}),
                                                                                      out_vec.data());

        for (size_t j = 0u; j < BATCH_SZ; ++j)
        {
            total += out_vec[j];
        }
    }

    auto clock_last                 = std::chrono::high_resolution_clock::now();
    intmax_t lapsed                 = std::chrono::duration_cast<std::chrono::milliseconds>(clock_last - clock_first).count();
    intmax_t nano_lapsed            = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_last - clock_first).count();
    size_t total_flops              = LOOP_SZ * BATCH_SZ * BASE_SZ * OPERATION_PER_COEFFICIENT;
    double ops_per_tick             = static_cast<double>(total_flops) / nano_lapsed;

    if (!std::isnan(total))
    {
        std::cout << "flops > " << total_flops << "<> lapsed > " << lapsed << "<ms>" << ops_per_tick << "<ops_per_tick>\n";
    }
}
int main()
{
    test_batch_8();
    test_batch_16();
    test_batch_32();
}