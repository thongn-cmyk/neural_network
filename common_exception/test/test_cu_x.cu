#include <cuda_management/cu_x.h>
#include <iostream>
#include <functional>
#include <random>
#include <utility>
#include <algorithm>
#include <chrono>
#include "assert.h"
#include <bit>

// #include <serializer/dg_buf.h> 

__global__ void kernel_test(char * data_buf, size_t * result_buf, size_t data_buf_sz)
{
    *result_buf = 0u;

    for (size_t i = 0u; i < data_buf_sz; ++i)
    {
        *result_buf += std::bit_cast<uint8_t>(data_buf[i]);
    }
}

void run_host_test(char * data_buf, size_t * result_buf, size_t data_buf_sz)
{
    *result_buf = 0u;

    for (size_t i = 0u; i < data_buf_sz; ++i)
    {
        *result_buf += std::bit_cast<uint8_t>(data_buf[i]);
    }
}

auto randomize_size(size_t sz) -> size_t
{
    if (sz == 0u)
    {
        std::abort();
    }

    static auto random_device = std::bind(std::uniform_int_distribution<size_t>{},
                                          std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    
    return random_device() % sz;
}

auto randomize_buffer(size_t sz) -> std::vector<char>
{
    std::vector<char> buf(sz);

    std::generate(buf.begin(), buf.end(), []() -> char
    {
        return static_cast<char>(randomize_size(256));
    });

    return buf;
}

void test_cuda_buffer_from_size()
{
    const size_t BUF_SZ = randomize_size(size_t{1} << 4);

    auto host_buf       = randomize_buffer(BUF_SZ);
    auto data_buf       = cu_x::make_cuda_buffer_from_size(BUF_SZ);
    auto result_buf     = cu_x::make_cuda_buffer_from_size(sizeof(size_t));
    size_t host_result  = {};

    cudaMemcpy(data_buf.get(), host_buf.data(), BUF_SZ, cudaMemcpyHostToDevice);

    kernel_test<<<1, 1>>>(data_buf.get(), reinterpret_cast<size_t *>(result_buf.get()), BUF_SZ);
    cudaDeviceSynchronize();

    run_host_test(host_buf.data(), &host_result, BUF_SZ);

    size_t device_result{};
    cudaMemcpy(&device_result, result_buf.get(), sizeof(size_t), cudaMemcpyDeviceToHost);

    assert(host_result == device_result);
}

void test_cuda_buffer_from_host_view()
{
    const size_t BUF_SZ = randomize_size(size_t{1} << 4);

    auto host_buf       = randomize_buffer(BUF_SZ);
    auto data_buf       = cu_x::make_cuda_buffer_from_host_view(std::string_view(host_buf.data(), BUF_SZ));
    auto result_buf     = cu_x::make_cuda_buffer_from_size(sizeof(size_t));
    size_t host_result  = {};

    kernel_test<<<1, 1>>>(data_buf.get(), reinterpret_cast<size_t *>(result_buf.get()), BUF_SZ);
    cudaDeviceSynchronize();

    run_host_test(host_buf.data(), &host_result, BUF_SZ);

    size_t device_result{};
    cudaMemcpy(&device_result, result_buf.get(), sizeof(size_t), cudaMemcpyDeviceToHost);

    assert(host_result == device_result);
}

extern "C" void run_cuda_test()
{
    std::cout << "__BEGIN_CU_X_TEST__\n";

    const size_t TEST_SZ = size_t{1} << 20;
    const size_t COUT_SZ = size_t{1} << 8;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_cuda_buffer_from_size();
        test_cuda_buffer_from_host_view();

        if (i % COUT_SZ == 0u)
        {
            std::cout << "Progress: " << (i * 100.0 / TEST_SZ) << "%\n";
        }
    }

    std::cout << "__END_CU_X_TEST__\n";
}