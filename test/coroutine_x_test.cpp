#define STRONG_MEMORY_ORDERING_FLAG true

#include <iostream>
// #include "coroutine_x.h"
#include <coroutine_subsystem/coroutine_x.h>
#include <utility>
#include <functional>
#include <random>
#include <algorithm>
#include <stdint.h>
#include <stdlib.h>
#include <chrono>

class Counter: public virtual coroutine_x::CoroutineableInterface
{
    private:

        size_t * counter;
        size_t expected_value;

    public:

        Counter(size_t * counter,
                size_t expected_value): counter(counter),
                                        expected_value(expected_value){}

        auto next() noexcept -> bool
        {
            *this->counter += 1;

            return true;
        }

        auto has_next() noexcept -> bool
        {
            return *this->counter < this->expected_value;
        }
};

class Counter2: public virtual coroutine_x::CoroutineableInterface
{
    private:

        using randomizer_t = decltype(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(0)}));

        size_t * counter;
        size_t expected_value;

        randomizer_t randomizer;
        
    public:

        Counter2(size_t * counter,
                 size_t expected_value): counter(counter),
                                         expected_value(expected_value),
                                         randomizer(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())})){}

        auto next() noexcept -> bool
        {
            *this->counter += 1;

            return this->randomizer() % 2 == 0u;
        }

        auto has_next() noexcept -> bool
        {
            return *this->counter < this->expected_value;
        }
};

void test_coroutine_no_delay()
{
    std::cout << "__BEGIN_TEST_COROUTINE_NO_DELAY__" << std::endl;

    cron_subsystem::init();
    coroutine_x::init();

    size_t counter          = 0u;
    size_t expected_value   = size_t{1} << 20;

    std::shared_ptr<Counter> counter_2 = std::make_shared<Counter>(&counter, expected_value);
    coroutine_x::run_promise(counter_2, coroutine_x::COMPUTE_COROUTINE).wait();

    std::cout << "counter > " << counter << " expected value > " << expected_value << std::endl;

    coroutine_x::deinit();
    cron_subsystem::deinit();

    std::cout << "__END_TEST_COROUTINE_NO_DELAY__" << std::endl;
}

void test_coroutine_random_delay()
{
    std::cout << "__BEGIN_TEST_COROUTINE_RANDOM_DELAY__" << std::endl;

    cron_subsystem::init();
    coroutine_x::init();

    size_t counter          = 0u;
    size_t expected_value   = size_t{1} << 10;

    std::shared_ptr<Counter2> counter_2 = std::make_shared<Counter2>(&counter, expected_value);
    coroutine_x::run_promise(counter_2, coroutine_x::COMPUTE_COROUTINE).wait();

    std::cout << "counter > " << counter << " expected value > " << expected_value << std::endl;

    coroutine_x::deinit();
    cron_subsystem::deinit();

    std::cout << "__END_TEST_COROUTINE_RANDOM_DELAY__" << std::endl;
}

class Counter3: public virtual coroutine_x::CoroutineableInterface
{
    private:

        using randomizer_t = decltype(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(0)}));

        std::shared_ptr<size_t> counter;
        randomizer_t randomizer;
    
    public:

        Counter3(std::shared_ptr<size_t> counter): counter(counter),
                                                   randomizer(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())})){}

        auto next() noexcept -> bool
        {
            *this->counter -= 1;

            return this->randomizer() % 2 == 0u;
        }

        auto has_next() noexcept -> bool
        {
            return *this->counter > 0u;
        }
};

auto get_random_coroutine_topic() -> uint8_t
{
    static auto randomizer              = std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())});
    const size_t TOPIC_ENUMERATION_SZ   = 3u;
    size_t topic_enumeration            = randomizer() % TOPIC_ENUMERATION_SZ;

    switch (topic_enumeration)
    {
        case 0:
        {
            return coroutine_x::NETWORK_COROUTINE;
        }
        case 1:
        {
            return coroutine_x::FILEIO_COROUTINE;
        }
        case 2:
        {
            return coroutine_x::COMPUTE_COROUTINE;
        }
        default:
        {
            std::abort();
        }
    }
}

void test_one_coroutine_mixed()
{
    cron_subsystem::init();
    coroutine_x::init();

    static auto randomizer              = std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())});
    const size_t CONCURRENCY_SZ_RANGE   = size_t{1} << 10;
    const size_t EXPECTED_VALUE_RANGE   = size_t{1} << 10;

    size_t concurrency_sz               = randomizer() % CONCURRENCY_SZ_RANGE;

    std::vector<std::pair<std::shared_ptr<coroutine_x::CoroutineableInterface>, std::shared_ptr<size_t>>> test_vec{};

    for (size_t i = 0u; i < concurrency_sz; ++i)
    {
        size_t expected_value = randomizer() % EXPECTED_VALUE_RANGE;
        std::shared_ptr<size_t> counter = std::make_shared<size_t>(expected_value);
        std::shared_ptr<coroutine_x::CoroutineableInterface> coroutineable = std::make_shared<Counter3>(counter);

        test_vec.push_back({coroutineable, counter});
    }

    std::vector<std::pair<coroutine_x::CoroutineWaiter, std::shared_ptr<size_t>>> waiter_vec{};

    for (const auto& [coroutineable, counter]: test_vec)
    {
        waiter_vec.push_back({coroutine_x::run_promise(coroutineable, get_random_coroutine_topic()), counter});
    }

    std::shuffle(waiter_vec.begin(), waiter_vec.end(), std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())});

    for (auto& [waitable, expected_value]: waiter_vec)
    {
        waitable.wait();

        if (*expected_value != 0u)
        {
            std::cout << "mayday, mismatched value" << std::endl;
            std::abort();
        }
    }

    coroutine_x::deinit();
    cron_subsystem::deinit();
}

void test_coroutine_mixed()
{
    const size_t TEST_SZ    = size_t{1} << 10;
    const size_t COUT_SZ    = size_t{1} << 0;

    std::cout << "__BEGIN_TEST_COROUTINE_MIXED__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_coroutine_mixed();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_TEST_COROUTINE_MIXED__" << std::endl;
}

void run_test()
{
    test_coroutine_no_delay();
    test_coroutine_random_delay();
    test_coroutine_mixed();
}

int main()
{
    run_test();
}