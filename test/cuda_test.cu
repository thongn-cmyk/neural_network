#include <iostream>
#include <memory>
#include "assert.h"
#include <cuda_management/cu_x.h>
#include <functional>
#include <algorithm>
#include <numeric>

template <class T>
class CudaMemoryAllocator
{
    public:

        using value_type                                = T;
        using pointer                                   = T*;
        using const_pointer                             = const T*;
        using reference                                 = T&;
        using const_reference                           = const T&;
        using size_type                                 = std::size_t;
        using difference_type                           = std::ptrdiff_t;
        using propagate_on_container_move_assignment    = std::true_type;
        using is_always_equal                           = std::true_type; // Stateless

        template <class U>
        struct rebind
        {
            using other = CudaMemoryAllocator<U>; 
        };

        constexpr CudaMemoryAllocator() noexcept = default;
        constexpr CudaMemoryAllocator(const CudaMemoryAllocator&) = default;

        template <class U, std::enable_if_t<std::negation_v<std::is_same<T, U>>, bool> = true>
        constexpr CudaMemoryAllocator(const CudaMemoryAllocator<U>&){}

        ~CudaMemoryAllocator() = default;

        auto allocate(std::size_t n) -> pointer
        {
            size_t total_mem = n * sizeof(T);

            void * ptr;
            cudaError_t err = cudaMalloc(&ptr, total_mem);

            if (err != cudaSuccess)
            {
                assert(false);
            }

            return static_cast<pointer>(ptr);
        }

        void deallocate(T * p, std::size_t n) noexcept
        {
            cudaFree(p);
        }
};

__global__ void cuda_hello()
{
    cu_x::vector<size_t> test_vec{};

    test_vec.resize(10);
    std::iota(test_vec.begin(), test_vec.end(), 0);

    size_t total = 0u;

    for (size_t e: test_vec)
    {
        total += e;
    }

    printf("Hello World from GPU! Value: %zu\n", total);
}

int main()
{

    cuda_hello<<<1, 5>>>(); // Launch 5 threads on 1 block

    {
        auto err = cudaGetLastError();

        if (err != cudaSuccess)
        {
            printf("CUDA Error: %s\n", cudaGetErrorString(err));
        }
    }

    {
        auto err = cudaDeviceSynchronize();

        if (err != cudaSuccess)
        {
            printf("CUDA Error: %s\n", cudaGetErrorString(err));
        }
    }

    // cudaDeviceSynchronize();
    return 0;
}
