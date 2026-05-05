#include <serializer/dg_buf.h>
#include <cuda_management/host_service_header.h>
#include <iostream>
#include <type_traits>

template <class T>
__global__ void test_vector(T vec, size_t expected_value)
{
    size_t total = 0u;

    for (auto c: vec)
    {
        total += c;
    }

    assert(total == expected_value);
}

template <class T>
__global__ void test_2d_vector(T vec, size_t expected_value)
{
    size_t total = 0u;
    
    for (auto sub_vec: vec)
    {
        for (auto c: sub_vec)
        {
            total += c;
        }
    }

    assert(total == expected_value);
}

void host_test_vector()
{
    std::vector<size_t> total_vec{1, 2, 3, 4, 5, 6};
    std::string bstream = {}; 

    auto cuda_total_vec = dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(total_vec, bstream);
    auto cuda_buf       = cuda_management::host_service::make_cuda_buffer_from_host_view(bstream);
    auto host_buf       = cuda_management::host_service::cuda_to_host_buffer(cuda_buf, bstream.size());

    if (std::memcmp(host_buf.get(), bstream.data(), bstream.size() + 1) != 0)
    {
        std::cout << "mayday, mismatched memory\n";
        std::abort();
    }

    cuda_total_vec.set_buf(bstream.data());

    size_t total = 0u;

    for (auto c: cuda_total_vec)
    {
        total += c;
    }

    assert(total == 21);

    cuda_total_vec.set_buf(cuda_buf.get());

    test_vector<<<1, 1>>>(cuda_total_vec, 21);
    cudaDeviceSynchronize();
}

void host_test_2d_vector()
{
    std::vector<std::vector<size_t>> total_vec{{1, 2}, {3, 4}, {5, 6}};
    std::string bstream = {};

    auto cuda_total_vec = dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(total_vec, bstream);
    auto cuda_buf       = cuda_management::host_service::make_cuda_buffer_from_host_view(bstream);
    auto host_buf       = cuda_management::host_service::cuda_to_host_buffer(cuda_buf, bstream.size());

    if (std::memcmp(host_buf.get(), bstream.data(), bstream.size() + 1) != 0)
    {
        std::cout << "mayday, mismatched memory\n";
        std::abort();
    }

    cuda_total_vec.set_buf(bstream.data());

    size_t total = 0u;

    for (auto sub_vec: cuda_total_vec)
    {
        for (auto c: sub_vec)
        {
            total += c;
        }
    }

    assert(total == 21);

    cuda_total_vec.set_buf(cuda_buf.get());

    test_2d_vector<<<1, 1>>>(cuda_total_vec, 21);
    cudaDeviceSynchronize();    
}

void run_test()
{
    std::cout << "__BEGIN_CU_DG_BUF_TEST__\n";
    std::cout << "testing host vector...\n";
    host_test_vector();
    std::cout << "testing host 2d vector...\n";
    host_test_2d_vector();
    std::cout << "__END_CU_DG_BUF_TEST__\n";
}

int main()
{
    run_test();
}