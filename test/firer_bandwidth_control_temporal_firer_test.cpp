#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <fire_bandwidth_control/temporal_firer.h>
#include <iostream>
#include <random>
#include <functional>
#include <utility>
#include <algorithm>
#include <thread>

class CounterFireable: public virtual fire_bandwidth_control::interface::FireableInterface
{
    private:

        size_t expected_counter;
        size_t * counter;

    public:

        CounterFireable(size_t expected_counter,
                        size_t * counter): expected_counter(expected_counter),
                                           counter(counter){}

        auto fire_one(common_exception::CancellationTokenInterface& cancellation_token) -> bool
        {
            if (*this->counter == this->expected_counter)
            {
                return false;
            }

            *this->counter += 1;

            return true;
        }
};

auto randomize_int(size_t range) -> size_t
{
    if (range == 0u)
    {
        std::abort();
    }

    static auto random_device = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return random_device() % range;
}

auto get_random_config() -> fire_bandwidth_control::temporal_firer::TemporalFirerConfig
{
    return 
    {
        .window_population  = static_cast<uint64_t>(randomize_int(size_t{1} << 2)),
        .window_dur         = std::chrono::nanoseconds(randomize_int(size_t{1} << 20))
    };
}

auto get_external_random_config() -> fire_bandwidth_control::temporal_firer::ExternalTemporalFirerConfig
{
    return fire_bandwidth_control::temporal_firer::to_external_temporal_firer_config(get_random_config());
}

void test_fireable(size_t counting_value)
{
    size_t counter = 0u;

    fire_bandwidth_control::temporal_firer::TemporalFirer firer(get_external_random_config());
    CounterFireable fireable(counting_value, &counter);
    common_exception::CancellationToken cancellation_token{};

    firer.run(fireable, cancellation_token);

    if (counter != counting_value)
    {
        std::cout << "mayday, test_zero_fireable failed, count mismatched\n";
        std::abort();
    }
}

void test_zero_fireable()
{
    test_fireable(0u);
}

void test_ten_fireable()
{
    test_fireable(10u);
}

class CancellableFireable: public virtual fire_bandwidth_control::interface::FireableInterface
{
    public:

        auto fire_one(common_exception::CancellationTokenInterface& cancellation_token) -> bool
        {
            if (randomize_int(100) == 0u)
            {
                common_exception::throw_exception(common_exception::OPERATION_CANCELED_ERROR);
            }

            return true;
        }
};

void test_cancellable_fireable()
{
    fire_bandwidth_control::temporal_firer::TemporalFirer firer(get_external_random_config());
    auto fireable = CancellableFireable();
    common_exception::CancellationToken cancellation_token{};

    try
    {
        firer.run(fireable, cancellation_token);
    }
    catch (common_exception::operation_canceled_error& e)
    {
        return;
    }
}

class ExceptableFireable: public virtual fire_bandwidth_control::interface::FireableInterface
{
    public:

        auto fire_one(common_exception::CancellationTokenInterface& cancellation_token) -> bool
        {
            if (randomize_int(100) == 0u)
            {
                throw std::runtime_error("something went wrong");
            }

            return true;
        }
};

void test_excepted_fireable()
{
    fire_bandwidth_control::temporal_firer::TemporalFirer firer(get_external_random_config());
    auto fireable = ExceptableFireable();
    common_exception::CancellationToken cancellation_token{};

    try
    {
        firer.run(fireable, cancellation_token);
    }
    catch (std::runtime_error& e)
    {
        if (std::string(e.what()) == std::string("something went wrong"))
        {
            return;
        }

        std::cout << std::string(e.what());
        throw;
    }
}

void test_loop()
{
    const size_t TEST_SZ    = size_t{1} << 4;
    const size_t COUT_SZ    = size_t{1} << 0;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_fireable(randomize_int(size_t{1} << 4));
        test_cancellable_fireable();
        test_excepted_fireable();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }
}

void run_test()
{
    std::cout << "__BEGIN_FIRER_BANDWIDTH_CONTROL_TEMPORAL_FIRER_TEST__\n";

    std::cout << "testing zero fireable...\n";
    test_zero_fireable();

    std::cout << "testing ten fireable...\n";
    test_ten_fireable();

    std::cout << "testing cancellable fireable...\n";
    test_cancellable_fireable();

    std::cout << "testing excepted fireable...\n";
    test_excepted_fireable();

    std::cout << "testing all range fireable...\n";
    test_loop();

    std::cout << "__END_FIRER_BANDWIDTH_CONTROL_TEMPORAL_FIRER_TEST__\n";
}

int main()
{
    run_test();
}