#define DEBUG_MODE_FLAG true
#define STRONG_MEMORY_ORDERING_FLAG true

#include <main_service/thread_service.h>
#include <main_service/main_broker.h>
#include <main_service/main_service.h>
#include <iostream>

class SumTask: public virtual main_service::thread_service::TaskInterface
{
    private:
    
        size_t * counter;
        size_t counting_sz;

    public:

        SumTask(size_t * counter,
                size_t counting_sz): counter(counter),
                                     counting_sz(counting_sz){}

        void run(common_exception::CancellationTokenInterface& cancellation_token) noexcept
        {
            for (size_t i = 0u; i < this->counting_sz; ++i)
            {
                *this->counter += 1;
            }
        }
};

class InfiniteSumTask: public virtual main_service::thread_service::TaskInterface
{
    private:

        size_t * counter;
    
    public:

        InfiniteSumTask(size_t * counter): counter(counter){}

        void run(common_exception::CancellationTokenInterface& cancellation_token) noexcept
        {
            while (true)
            {
                *this->counter += 1u;

                if (cancellation_token.is_canceled())
                {
                    return;
                }
            }
        }
};

void thread_buyer()
{
    size_t seed                         = std::bit_cast<size_t>(std::this_thread::get_id());
    const size_t CAP                    = size_t{1} << 10;
    size_t counting_value               = seed % CAP;
    size_t counter                      = 0u;

    std::shared_ptr<std::thread> thr    = main_service::broke_thread(std::make_unique<SumTask>(&counter, counting_value));

    if (counting_value % 2 == 0u)
    {
        thr->join();
    }
    else
    {
        thr = nullptr;
    }

    if (counter != counting_value)
    {
        std::cout << "mayday, mismatched counter value\n";
        std::abort();
    }
    else
    {
        std::cout << "Success 0\n";
    }

    size_t counter_2                    = 0u;
    std::shared_ptr<std::thread> thr_2  = main_service::broke_thread(std::make_unique<InfiniteSumTask>(&counter_2));

    std::this_thread::sleep_for(std::chrono::seconds(1));

    thr_2 = nullptr;

    if (counter_2 == 0u)
    {
        std::cout << "mayday, mismatched counter value\n";
        std::abort();
    }
    else
    {
        std::cout << "Success 1\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
}

int main()
{
    const size_t THREAD_BUYER_SZ = size_t{1} << 4;

    main_service::init();
    std::vector<std::thread> thr_vec{};

    for (size_t i = 0u; i < THREAD_BUYER_SZ; ++i)
    {
        thr_vec.emplace_back(thread_buyer);
    }

    main_service::main_subscribe();
}