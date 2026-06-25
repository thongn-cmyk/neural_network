//__GIT_INTEGRATION_TAG__

#define STRONG_MEMORY_ORDERING_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <optional>
#include <unordered_map>
#include <random>
#include <functional>
#include <utility>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <unordered_set>

#ifndef __CUDA_MEMORY_CONFIG_H__
#define __CUDA_MEMORY_CONFIG_H__

namespace global_config::cuda_memory_config
{
    static inline constexpr uint64_t CUDA_HEAP_MEMORY_SZ    = uint64_t{1} << 12;
    static inline constexpr uint64_t CUDA_HEAP_LEAF_SZ      = size_t{1} << 6;
    static inline constexpr bool CUDA_HAS_HEAP              = true;
}

#endif

#include <cuda_management/cuda_malloc.h>

auto randomize_int(size_t first, size_t last) -> size_t
{
    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{first, last}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return first + randomizer() % (last - first);
}

static std::unordered_set<uintptr_t> occupied_byte_addr_set{};

static std::optional<uintptr_t> min_addr_ptr = std::nullopt;
static std::optional<uintptr_t> max_addr_ptr = std::nullopt; 

auto allocate_one_buffer(size_t sz) -> std::shared_ptr<void>
{
    void * ptr;

    try
    {
        ptr = cuda_management::cuda_malloc::malloc(sz);
    }
    catch (...)
    {
        return nullptr;
    }

    for (size_t i = 0u; i < sz; ++i)
    {
        uintptr_t byte_addr = reinterpret_cast<uintptr_t>(ptr) + i;

        if (occupied_byte_addr_set.find(byte_addr) != occupied_byte_addr_set.end())
        {
            std::cout << "mayday, memory overlap detected\n";
            std::abort();
        }

        occupied_byte_addr_set.insert(byte_addr);
    }

    if (ptr == nullptr)
    {
        return nullptr;
    }

    if (!min_addr_ptr.has_value() || reinterpret_cast<uintptr_t>(ptr) < min_addr_ptr.value())
    {
        min_addr_ptr = reinterpret_cast<uintptr_t>(ptr);
    }

    if (!max_addr_ptr.has_value() || reinterpret_cast<uintptr_t>(ptr) + sz > max_addr_ptr.value())
    {
        max_addr_ptr = reinterpret_cast<uintptr_t>(ptr) + sz;
    }

    if (max_addr_ptr.value() - min_addr_ptr.value() > global_config::cuda_memory_config::CUDA_HEAP_MEMORY_SZ)
    {
        std::cout << "heap_sz > " << global_config::cuda_memory_config::CUDA_HEAP_MEMORY_SZ << " bytes <>" << "max_addr_ptr - min_addr_ptr = " << max_addr_ptr.value() - min_addr_ptr.value() << " bytes\n";
        std::cout << "mayday, memory overflow detected\n";
        std::abort();
    }

    return std::shared_ptr<void>(ptr, [=](void * ptr)
    {
        cuda_management::cuda_malloc::free(ptr);

        for (size_t i = 0u; i < sz; ++i)
        {
            uintptr_t byte_addr = reinterpret_cast<uintptr_t>(ptr) + i;
            occupied_byte_addr_set.erase(byte_addr);
        }
    });
}

void run_test()
{   
    const size_t TEST_SZ                = size_t{1} << 30;
    const size_t COUT_SZ                = size_t{1} << 10;      
    const size_t ALLOCATION_FREE_CHANCE = size_t{1} << 4;
    const size_t MAX_ALLOC_SZ           = size_t{1} << 4;

    std::vector<std::shared_ptr<void>> allocated_buffer_vec{};

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        allocated_buffer_vec.push_back(allocate_one_buffer(randomize_int(0u, MAX_ALLOC_SZ)));

        if (allocated_buffer_vec.back() == nullptr)
        {
            allocated_buffer_vec.clear();
        }

        if (randomize_int(0u, ALLOCATION_FREE_CHANCE) == 0u)
        {
            allocated_buffer_vec.clear();
        }

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << " iterations completed\n";
        }
    }
}

void initialize_resource()
{
    std::cout << "initializing resource...\n";

    cuda_management::cuda_malloc::init();
}

int main()
{
    initialize_resource();
    run_test();
}