#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <iostream>
#include <internal_rest/network_ssl_symmetric_encoder.h>
#include <random>
#include <functional>
#include <algorithm>
#include <chrono>
#include <bit>

auto randomize_size(size_t sz_range) -> size_t
{
    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return randomizer() % sz_range;
}

auto randomize_string(size_t sz) -> std::string
{
    std::string rs{};

    for (size_t i = 0u; i < sz; ++i)
    {
        rs.push_back(std::bit_cast<char>(static_cast<uint8_t>(randomize_size(256))));
    }

    return rs;
}

auto get_random_secret() -> std::string
{
    return randomize_string(size_t{1} << 4);
}

auto get_random_one_test_size() -> size_t
{
    return randomize_size(size_t{1} << 4);
}

auto get_random_per_token_size() -> size_t
{ 
    return randomize_size(size_t{1} << 4);
}

auto get_random_token()
{
    return randomize_string(size_t{1} << 4);
}

void run_one_test()
{
    using namespace dg_sock::ud_sym_encoder;

    std::string secret  = get_random_secret();
    size_t sz           = get_random_one_test_size();
    size_t per_token_sz = get_random_per_token_size();

    DoubleEncoder encoder(secret, per_token_sz);

    for (size_t i = 0u; i < sz; ++i)
    {
        std::string token = get_random_token();

        if (token != encoder.decode(encoder.encode(token)))
        {
            std::cout << "mayday, mismatched value\n";
            std::abort();
        }
    }
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_SYM_ENCODER_TEST__\n";

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    std::cout << "__END_SYM_ENCODER_TEST__\n";
}

int main()
{
    run_test();
}