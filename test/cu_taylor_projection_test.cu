#include <cuda_matrix/taylor_projection.h>
#include <algorithm>

__device__ float getnan(float x)
{
    return nanf("");
}

__global__ void test_taylor_projection()
{
    float x{};
    float coeff_arr[5] = {1, 1, 1, 1, 1};
    float result = cuda_matrix::taylor_projection::base_taylor_project(x, coeff_arr, cuda_matrix::utility::to_size_container(5));

    assert(result != getnan(result));
}

__global__ void test_batch_taylor_projection()
{
    float x_arr[3] = {0.0f, 0.0f, 0.0f};
    float coeff_arr[5] = {1, 1, 1, 1, 1};
    float y_arr[3]{};

    cuda_matrix::taylor_projection::base_batch_taylor_project(x_arr, cuda_matrix::utility::to_size_container(3), coeff_arr, cuda_matrix::utility::to_size_container(5), y_arr);

    assert((y_arr[0] != getnan(y_arr[0])));
    assert((y_arr[1] != getnan(y_arr[1]))) ;
    assert((y_arr[2] != getnan(y_arr[2])));
}

__global__ void test_get_multivariate_taylor_projection_coefficient_size()
{
    size_t coeff_sz = cuda_matrix::taylor_projection::get_multivariate_taylor_projection_coefficient_size(3, 5);

    assert(coeff_sz != 0u);
}


__global__ void test_multivariate_taylor_projection()
{
    float x_arr[3] = {0.0f, 0.0f, 0.0f};
    float coeff_arr[27] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    size_t coeff_arr_offset = 0u;

    float result = cuda_matrix::taylor_projection::multivariate_taylor_project(x_arr, cuda_matrix::utility::to_size_container(3),
                                                                               cuda_matrix::utility::to_size_container(3),
                                                                               coeff_arr, coeff_arr_offset, 27u);

    assert((result != getnan(result)));
}

__global__ void test_get_multidimensional_taylor_projection_coefficient_size()
{
    size_t coeff_sz = cuda_matrix::taylor_projection::get_multidimensional_taylor_projection_coefficient_size(3, 5, 3, false);

    assert(coeff_sz != 0u);
}


__global__ void test_multidimensional_taylor_projection()
{
    float x_arr[3] = {0.0f, 0.0f, 0.0f};
    float coeff_arr[81]{};
    float y_arr[3]{};

    std::fill(coeff_arr, coeff_arr + 81, 1.0f);
    size_t coeff_arr_offset = 0u;

    cuda_matrix::taylor_projection::multidimensional_taylor_project(x_arr, cuda_matrix::utility::to_size_container(3),
                                                                    cuda_matrix::utility::to_size_container(3),
                                                                    coeff_arr, coeff_arr_offset, 81u,
                                                                    y_arr, 3);

    assert((y_arr[0] != getnan(y_arr[0])));
    assert((y_arr[1] != getnan(y_arr[1])));
    assert((y_arr[2] != getnan(y_arr[2])));
}

__global__ void test_get_batch_multivariate_taylor_projection_coefficient_size()
{
    size_t coeff_sz = cuda_matrix::taylor_projection::get_batch_multivariate_taylor_projection_coefficient_size(3, 5, 3);

    assert(coeff_sz != 0u);
}

__global__ void test_batch_multivariate_taylor_projection()
{
    float x_arr[9] = {0.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 0.0f};

    float coeff_arr[27]{};
    float y_arr[3]{};

    std::fill(coeff_arr, coeff_arr + 27, 0.0f);
    std::fill(y_arr, y_arr + 3, 0.0f);

    size_t coeff_arr_offset = 0u;
    
    cuda_matrix::taylor_projection::batch_multivariate_taylor_project(x_arr, cuda_matrix::utility::to_size_container(std::integral_constant<size_t, 3u>{}), cuda_matrix::utility::to_size_container(std::integral_constant<size_t, 3u>{}),
                                                                      cuda_matrix::utility::to_size_container(std::integral_constant<size_t, 3u>{}),
                                                                      coeff_arr, coeff_arr_offset, 27u,
                                                                      y_arr);

    assert((y_arr[0] != getnan(y_arr[0])));
    assert((y_arr[1] != getnan(y_arr[1])));
    assert((y_arr[2] != getnan(y_arr[2])));
}

__global__ void test_get_batch_multidimensional_taylor_projection_coefficient_size()
{
    size_t coeff_sz = cuda_matrix::taylor_projection::get_batch_multidimensional_taylor_projection_coefficient_size(3, 5, 3, 3);

    assert(coeff_sz != 0u);
}

__global__ void test_batch_multidimensional_taylor_projection()
{
    float x_arr[9] = {0.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 0.0f};

    float coeff_arr[81]{};
    float y_arr[9]{};

    std::fill(coeff_arr, coeff_arr + 81, 0.0f);
    std::fill(y_arr, y_arr + 9, 0.0f);

    size_t coeff_arr_offset = 0u;

    cuda_matrix::taylor_projection::batch_multidimensional_taylor_project(x_arr, cuda_matrix::utility::to_size_container(std::integral_constant<size_t, 3u>{}), cuda_matrix::utility::to_size_container(std::integral_constant<size_t, 3u>{}),
                                                                          cuda_matrix::utility::to_size_container(std::integral_constant<size_t, 3u>{}),
                                                                          coeff_arr, coeff_arr_offset, 81u,
                                                                          y_arr, 3u);

    assert((y_arr[0] != getnan(y_arr[0])));
    assert((y_arr[1] != getnan(y_arr[1])));
    assert((y_arr[2] != getnan(y_arr[2])));
    assert((y_arr[3] != getnan(y_arr[3])));
    assert((y_arr[4] != getnan(y_arr[4])));
    assert((y_arr[5] != getnan(y_arr[5])));
    assert((y_arr[6] != getnan(y_arr[6])));
    assert((y_arr[7] != getnan(y_arr[7])));
    assert((y_arr[8] != getnan(y_arr[8])));
}

int main()
{
    test_taylor_projection<<<1, 1>>>();
    test_batch_taylor_projection<<<1, 1>>>();
    test_get_multivariate_taylor_projection_coefficient_size<<<1, 1>>>();
    test_multivariate_taylor_projection<<<1, 1>>>();
    test_get_multidimensional_taylor_projection_coefficient_size<<<1, 1>>>();
    test_multidimensional_taylor_projection<<<1, 1>>>();
    test_get_batch_multivariate_taylor_projection_coefficient_size<<<1, 1>>>();
    test_batch_multivariate_taylor_projection<<<1, 1>>>();
    test_get_batch_multidimensional_taylor_projection_coefficient_size<<<1, 1>>>();
    test_batch_multidimensional_taylor_projection<<<1, 1>>>();

    cudaDeviceSynchronize();
}