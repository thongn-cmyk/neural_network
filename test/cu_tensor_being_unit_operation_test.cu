#include <taylor_matrix/cuda_matrix/tensor_being_unit_operation.h>
#include <cuda_management/scope_allocator.h>

using namespace taylor_matrix::cuda_matrix::tensor_being_unit_operation;

using Allocator = cuda_management::scope_allocator::SplitStackAllocator<>;

__global__ void test_allocate()
{
    allocate({}, Allocator{});
}

__global__ void test_deallocate()
{
    deallocate({}, Allocator{});
}

__global__ void test_flatten_to()
{
    flatten_to({}, {});
}

__global__ void test_flatten_size()
{
    flatten_size({});
}

__global__ void test_empty_as()
{
    empty_as({}, Allocator{});
}

__global__ void test_copy()
{
    copy({}, Allocator{});
}

__global__ void test_copy_to()
{
    copy_to({}, {});
}

__global__ void test_unflatten_to()
{
    unflatten_to({}, {});
}

__global__ void test_two_to_one_project()
{
    size_t sz{};

    two_to_one_project({},
                       {},
                       to_size_container(std::integral_constant<size_t, 1>{}),
                       {}, sz, {},
                       Allocator{});
}

__global__ void test_deparameterize()
{
    deparameterize({}, {}, Allocator{});
}

__global__ void test_accumulate()
{
    accumulate(std::add_pointer_t<BeingUnit>(),
               std::add_pointer_t<BeingUnit>(),
               Allocator{});
}

__global__ void test_accumulate_array()
{
    accumulate(std::add_pointer_t<BeingUnit *>(),
               {},
               Allocator{});
}

__global__ void test_div()
{
    div({},
        1,
        Allocator{});
}

__global__ void test_avg()
{
    avg(std::add_pointer_t<BeingUnit *>(),
        {},
        Allocator{});
}

int main()
{
    test_allocate<<<1, 1>>>();
    test_deallocate<<<1, 1>>>();
    test_flatten_to<<<1, 1>>>();
    test_flatten_size<<<1, 1>>>();
    test_empty_as<<<1, 1>>>();
    test_copy<<<1, 1>>>();
    test_copy_to<<<1, 1>>>();
    test_unflatten_to<<<1, 1>>>();
    test_two_to_one_project<<<1, 1>>>();
    test_deparameterize<<<1, 1>>>();
    test_accumulate<<<1, 1>>>();
    test_accumulate_array<<<1, 1>>>();
    test_div<<<1, 1>>>();
    test_avg<<<1, 1>>>();

    cudaDeviceSynchronize();

    return 0;
}