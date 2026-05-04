#include <taylor_matrix/cuda_matrix/tensor_process_group_operation.h>
#include <taylor_matrix/cuda_matrix/tensor_being_unit_operation.h>

__global__ void test_process_group()
{
    using namespace taylor_matrix::cuda_matrix::tensor_process_group_operation;
    size_t sz{};

    two_to_one_project({},
                       {},
                       to_size_container(std::integral_constant<size_t, 1u>{}),
                       {}, sz, {});
}

int main()
{
    test_process_group<<<1, 1>>>();
    cudaDeviceSynchronize();
}