#include <cuda_management/device_memory.h>
#include <cuda_management/cuda_vector.h>
#include <cuda_management/scope_allocator.h>

template <class ScopeAllocator>
__device__ void sum(size_t& value, size_t sz, ScopeAllocator&& allocator)
{
    using namespace cuda_management::scope_allocator;
    using namespace cuda_management::device_memory;

    scope_guard scope_guard(&allocator);

    size_t * arr = std_new_object<size_t>(allocator, sz);
    
    for (size_t i = 0u; i < sz; ++i)
    {
        arr[i] = i;
    }

    for (size_t i = 0u; i < sz; ++i)
    {
        value += i;
    }
} 

__global__ void test_cuda_allocator()
{
    // printf("Hello World from GPU! Value \n");

    using namespace cuda_management::device_memory;

    CudaAllocator allocator = CudaAllocator();

    int * arr   = std_new_array<int>(allocator, 10);
    int * obj   = std_new_object<int>(allocator);

    for (size_t i = 0u; i < 10u; ++i)
    {
        arr[i] = i;
        assert(arr[i] == i);
    }

    std_delete_object(allocator, obj);
    std_delete_array(allocator, arr);
}

__global__ void test_trivial_cuda_vector()
{
    using namespace cuda_management::cuda_vector;

    trivial_cuda_vector<int> vec{};

    for (size_t i = 0u; i < 1000; ++i)
    {
        vec.push_back(i);
        assert(vec.back() == i);
    }
}

__global__ void test_scope_allocator()
{
    using namespace cuda_management::scope_allocator;

    SplitStackAllocator allocator{};
    size_t total        = 0u;
    const size_t SUM_SZ = 100u;
    const size_t RUN_SZ = 100000u; 

    for (size_t i = 0u; i < RUN_SZ; ++i)
    {
        sum(total, SUM_SZ, allocator);
    }

    size_t other_total  = 0u;

    for (size_t i = 0u; i < RUN_SZ; ++i)
    {
        for (size_t j = 0u; j < SUM_SZ; ++j)
        {
            other_total += j;
        }
    }

    assert(total == other_total);
}

int main()
{
    test_cuda_allocator<<<1, 1>>>();
    test_trivial_cuda_vector<<<1, 1>>>();
    test_scope_allocator<<<1, 1>>>();

    cudaDeviceSynchronize();
}