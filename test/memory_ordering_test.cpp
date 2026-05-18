#include <stdint.h>
#include <stdlib.h>
#include <atomic>
#include <chrono>
#include <iostream>

__attribute__((noinline, noipa)) void increment(size_t * value)
{
    *value += 1;
}

int main()
{
    const size_t TEST_SZ = size_t{1} << 30;

    auto then       = std::chrono::high_resolution_clock::now();
    size_t value    = {};
    std::atomic<size_t> atomic_var{};

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        std::atomic_thread_fence(std::memory_order_acquire);
        increment(&value);
        std::atomic_thread_fence(std::memory_order_release);
    }

    auto now        = std::chrono::high_resolution_clock::now();
    auto lapsed     = std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count();

    std::cout << "lapsed > " << static_cast<uint64_t>(lapsed) << "<>" << value << "\n";
}
