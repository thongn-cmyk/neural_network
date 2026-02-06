#define STRONG_MEMORY_ORDERING_FLAG true

#include "coroutine_x.h"
#include <iostream>

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

int main()
{
    coroutine_x::init();

    size_t counter          = 0u;
    size_t expected_value   = size_t{1} << 20;

    std::shared_ptr<Counter> counter_2 = std::make_shared<Counter>(&counter, expected_value);
    coroutine_x::run_promise(counter_2, coroutine_x::COMPUTE_COROUTINE).wait();

    std::cout << "counter > " << counter << " expected value > " << expected_value << std::endl;

    coroutine_x::deinit();
}