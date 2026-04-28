#include <stdint.h>
#include <stdlib.h>
#include <concurrency_base/concurrency_base.h>

void initialize_resource()
{

}

class CalculationWorker: public virtual concurrency_base::InterruptableWorkerInterface
{
    private:

        size_t * counter;
        size_t range_sz;
        std::atomic<bool> * complete_var;

    public:

        CalculationWorker(size_t * counter,
                          size_t range_sz,
                          std::atomic<bool> * complete_var): counter(counter),
                                                             range_sz(range_sz),
                                                             complete_var(complete_var){}

        auto run_one_epoch(common_exception::CancellationTokenInterface& cancellation_token) -> bool
        {
            if (*this->counter == this->range_sz)
            {
                if (this->complete_var->load(std::memory_order_relaxed))
                {
                    return false;
                }

                this->complete_var->exchange(true, std::memory_order_release);
                return false;
            }

            (*this->counter)++;
        }
};

void test_calculation()
{

}

void test_error()
{

}

void test_cancellation()
{

}

void test_calculation_loop()
{

}

void run_test()
{
    std::cout << "__BEGIN_CONCURRENCY_BASE_ONFLY_TEST__\n";

    std::cout << "initializing resource...\n";
    initialize_resource();

    std::cout << "running compute test...\n";
    test_calculation();

    std::cout << "running error test...\n";
    test_error();

    std::cout << "running cancellation test...\n";
    test_cancellation();

    std::cout << "running loop test...\n";
    test_calculation_loop();

    std::cout << "__END_CONCURRENCY_BASE_ONFLY_TEST__\n";
}

int main()
{
    run_test();
}