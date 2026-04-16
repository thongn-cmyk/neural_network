#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <data_loader/hex_encoder/hex_encoder.h>
#include <iostream>
#include <random>
#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <chrono>

void run_one_test()
{
    const size_t RANDOM_SZ      = size_t{1} << 4;
    static auto random_device   = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    size_t test_sz              = random_device() % RANDOM_SZ;

    std::string test_str(test_sz, ' ');

    for (size_t i = 0u; i < test_sz; ++i)
    {
        test_str[i] = std::bit_cast<char>(static_cast<uint8_t>(random_device()));
    }

    if (test_str != data_loader::hex_encoder::hex_decode(data_loader::hex_encoder::hex_encode(test_str)))
    {
        std::cout << "mayday, mismatched serialization representation\n";
        std::abort();
    }
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 30;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_TESTING_HEX_ENCODER__\n";

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    std::cout << "__END_TESTING_HEX_ENCODER__\n";
}

int main()
{
    run_test();
}