#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <unordered_map>
#include <matrix_steering_subsystem/cosine_recommender_machine_x.h>
#include <cstdlib>
#include <new>

// Global operator new
static size_t allocated_sz  = 0u;
static size_t counter = 0u;

void* operator new(std::size_t size) 
{
    if (size == 0u)
    {
        return nullptr;
    }

    if (void* ptr = std::malloc(size))
    {
        allocated_sz += size;

        if (counter++ % 1024 == 0u)
        {
            std::cout << "allocated sz > " << allocated_sz << "\n";
        }

        return ptr;
    }

    throw std::bad_alloc(); // Standard requirement when allocation fails
}

// Global operator delete
void operator delete(void* ptr) noexcept
{
    if (ptr == nullptr)
    {
        return;
    }

    std::free(ptr);
}

int main()
{
    //this is too heavy

    using namespace cosine_recommender_machine_x;

    std::vector<std::unique_ptr<CosineRecommenderMachineInterface>> recommender_machine_vec{};

    const size_t TEST_SZ    = size_t{1} << 16;
    const size_t SPACE_SZ   = size_t{1} << 10;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        recommender_machine_vec.push_back(MachineFactory::get_best_recommender_machine(SPACE_SZ));

        if (i % 1024 == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    size_t c;

    std::cout << "machine size > " << recommender_machine_vec.size() << "\n";
    std::cin >> c;
    std::cout << "received > " << c << "\n";
}