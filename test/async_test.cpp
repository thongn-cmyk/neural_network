#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <seqpar_async/async_x.h>
#include <random>
#include <utility>
#include <algorithm>
#include <functional>
#include <stdint.h>
#include <stdlib.h>
#include <chrono>

void initialize_concurrency_base()
{
    using namespace concurrency_base;

    std::cout << "initializing concurrency base\n";
    std::vector<WorkerInformation> worker_info_vec{};

    for (size_t i = 0u; i < size_t{1} << 8; ++i)
    {
        worker_info_vec.push_back(WorkerInformation
        {
            .cpu_id = std::nullopt,
            .daemon = ASYNC_SEQPAR_DAEMON
        });
    }

    init(Config{worker_info_vec});
}

void test_one_async()
{
    const size_t WORKER_SZ_RANGE    = size_t{1} << 8;
    const size_t CONTAINER_SZ_RANGE = size_t{1} << 3;
    const size_t GROUP_COUNT_RANGE  = size_t{1} << 3;
    const size_t TEST_SZ_RANGE      = size_t{1} << 8;

    static auto randomizer          = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor    = std::uniform_int_distribution<size_t>{};

    size_t worker_sz                = uint_distributor(randomizer) % WORKER_SZ_RANGE + 1u;
    size_t container_sz             = uint_distributor(randomizer) % CONTAINER_SZ_RANGE;
    size_t group_count              = uint_distributor(randomizer) % GROUP_COUNT_RANGE + 1u;
    size_t test_sz                  = uint_distributor(randomizer) % TEST_SZ_RANGE;

    auto generator                  = [&]
    {
        return uint_distributor(randomizer);
    };

    async_x::init(worker_sz, container_sz);

    std::vector<size_t> random_vec(test_sz);
    std::atomic<size_t> total   = 0u;
    size_t expected_total       = 0u;

    std::generate(random_vec.begin(), random_vec.end(), generator);

    auto resolutor = [&](size_t e)
    {
        total.fetch_add(e, std::memory_order_relaxed);
    };

    auto resolutor2 = [&](size_t e)
    {
        expected_total += e;
    };

    async_x::sequential_parallel_group_launch_2(random_vec.begin(), random_vec.end(), resolutor, group_count);
    std::for_each(random_vec.begin(), random_vec.end(), resolutor2);

    if (total.load(std::memory_order_relaxed) != expected_total)
    {
        std::cout << "mayday, value mismatched" << std::endl;
        std::abort();
    }

    async_x::deinit();
}

void test_async()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 0;

    std::cout << "__BEGIN_ASYNC_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_async();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_ASYNC_TEST__" << std::endl;
}

int main()
{
    initialize_concurrency_base();
    test_async();
}