#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <iostream>
#include <internal_rest/network_ssl_symmetric_encoder.h>
#include <internal_rest/network_concurrency.h>
#include <internal_rest/network_randomizer.h>
#include <internal_rest/network_allocation.h>
#include <internal_rest/network_stack_allocation.h>
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

auto randomize_string(size_t sz) -> dg_sock::string
{
    dg_sock::string rs{};

    for (size_t i = 0u; i < sz; ++i)
    {
        rs.push_back(std::bit_cast<char>(static_cast<uint8_t>(randomize_size(256))));
    }

    return rs;
}

auto get_random_secret() -> dg_sock::string
{
    return randomize_string(size_t{1} << randomize_size(4));
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
    return randomize_string(randomize_size(size_t{1} << 4));
}

void run_one_test()
{
    using namespace dg_sock::ud_sym_encoder;

    dg_sock::string secret  = get_random_secret();
    size_t sz               = get_random_one_test_size();
    size_t per_token_sz     = get_random_per_token_size();

    DoubleEncoder encoder(secret, per_token_sz, size_t{1} << 6);

    for (size_t i = 0u; i < sz; ++i)
    {
        dg_sock::string token = get_random_token();

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

void bench_test(size_t salt_sz, size_t buffer_sz, size_t renew_sz)
{
    using namespace dg_sock::ud_sym_encoder;

    std::cout << "__BEGIN_SCOPE_BENCH_TEST__\n";
    std::cout << "testing > salt_sz: " << salt_sz << "<> buffer_sz: " << buffer_sz << "<> renew_sz: " << renew_sz << "\n";

    auto buffer = randomize_string(buffer_sz);
    DoubleEncoder encoder("", salt_sz, renew_sz);

    auto then   = std::chrono::high_resolution_clock::now();
    auto result = encoder.decode(encoder.encode(buffer));

    if (result != buffer)
    {
        std::cout << "mayday, mismatched string result\n";
        std::abort();
    }

    auto now    = std::chrono::high_resolution_clock::now();

    std::cout << "lapsed (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count() << "\n";
    std::cout << "__END_SCOPE_BENCH_TEST__\n";
}

void bench_test()
{
    bench_test(size_t{1} << 10, size_t{1} << 20, size_t{1} << 0);
    bench_test(size_t{1} << 10, size_t{1} << 20, size_t{1} << 2);
    bench_test(size_t{1} << 10, size_t{1} << 20, size_t{1} << 4);
    bench_test(size_t{1} << 10, size_t{1} << 20, size_t{1} << 6);
    // bench_test(size_t{1} << 10, size_t{1} << 30, size_t{1} << 2);
    bench_test(size_t{1} << 10, size_t{1} << 22, size_t{1} << 6);
}

void initialize_resource()
{
    std::cout << "initializing concurrency\n";
    dg_sock::network_concurrency::init({});

    std::cout << "initializing network randomizer\n";
    dg_sock::network_randomizer::init();

    std::cout << "initializing stack allocation\n";
    dg_sock::network_stack_allocation::init();
    
    std::cout << "initializing network allocation\n";
    dg_sock::network_allocation::init();
}

int main()
{
    initialize_resource();
    bench_test();
    run_test();
}