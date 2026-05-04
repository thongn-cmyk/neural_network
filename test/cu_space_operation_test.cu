#include <taylor_matrix/cuda_matrix/space_operation.h>
#include <stdio.h>
#include "assert.h"

__global__ void test_radian_normalize()
{
    float x = 10.0f;
    float result = taylor_matrix::cuda_matrix::space_operation::radian_normalize(x);
}

__global__ void test_restrict_scalar_mul_array()
{
    float arg_arr[5] = {1, 2, 3, 4, 5};
    float output_arr[5]{};
    float c = 2.0f;

    taylor_matrix::cuda_matrix::space_operation::restrict_scalar_mul_array(arg_arr, 5, c, output_arr);

    assert(output_arr[0] == 2.0f);
    assert(output_arr[1] == 4.0f);
    assert(output_arr[2] == 6.0f);
    assert(output_arr[3] == 8.0f);
    assert(output_arr[4] == 10.0f);
}

__global__ void test_restrict_scalar_div_array()
{
    float arg_arr[5] = {1, 2, 3, 4, 5};
    float output_arr[5]{};
    float c = 2.0f;

    taylor_matrix::cuda_matrix::space_operation::restrict_scalar_div_array(arg_arr, 5, c, output_arr);

    assert(output_arr[0] == 0.5f);
    assert(output_arr[1] == 1.0f);
    assert(output_arr[2] == 1.5f);
    assert(output_arr[3] == 2.0f);
}

__global__ void test_restrict_add_array()
{
    float lhs_arr[5] = {1, 2, 3, 4, 5};
    float rhs_arr[5] = {5, 4, 3, 2, 1};
    float output_arr[5]{};

    taylor_matrix::cuda_matrix::space_operation::restrict_add_array(lhs_arr, rhs_arr, 5, output_arr);

    assert(output_arr[0] == 6.0f);
    assert(output_arr[1] == 6.0f);
    assert(output_arr[2] == 6.0f);
    assert(output_arr[3] == 6.0f);
    assert(output_arr[4] == 6.0f);
}

__global__ void test_restrict_multidimensional_oval_to_euclidean_array()
{
    float radian_arr[3] = {0.0f, 0.0f, 0.0f};
    float radius_arr[3] = {1.0f, 1.0f, 1.0f};
    float output_arr[3]{};

    taylor_matrix::cuda_matrix::space_operation::restrict_multidimensional_oval_to_euclidean_array(radian_arr, 3, radius_arr, output_arr);
}

__global__ void test_dot_product()
{
    float lhs_arr[5] = {1, 2, 3, 4, 5};
    float rhs_arr[5] = {5, 4, 3, 2, 1};

    float result = taylor_matrix::cuda_matrix::space_operation::dot_product(lhs_arr, rhs_arr, 5);

    assert(result == 35.0f);
}

__global__ void test_coordinate_distance()
{
    float coor_arr[5] = {1, 2, 3, 4, 5};

    float result = taylor_matrix::cuda_matrix::space_operation::coordinate_distance(coor_arr, 5);

    assert((result - 7.416198f) <= 0.0001f);
}

__global__ void test_cosine_score()
{
    float coor_arr_1[5] = {1, 2, 3, 4, 5};
    float coor_arr_2[5] = {5, 4, 3, 2, 1};

    float result = taylor_matrix::cuda_matrix::space_operation::cosine_score(coor_arr_1, coor_arr_2, 5);
}

__global__ void test_cosine_angle()
{
    float coor_arr_1[5] = {1, 2, 3, 4, 5};
    float coor_arr_2[5] = {5, 4, 3, 2, 1};

    float result = taylor_matrix::cuda_matrix::space_operation::cosine_angle(coor_arr_1, coor_arr_2, 5);

    assert((result - 180.0f) <= 0.0001f);
}

__global__ void test_euclidean_to_radian_coordinate()
{
    float euclid_coor_arr[3] = {1.0f, 1.0f, 1.0f};
    float radian_coor_arr[3]{};

    taylor_matrix::cuda_matrix::space_operation::euclidean_to_radian_coordinate(euclid_coor_arr, 3, radian_coor_arr);
}

__global__ void test_radian_normalize_and_euclidean_to_radian_coordinate()
{
    float x = 10.0f;
    float radian_result = taylor_matrix::cuda_matrix::space_operation::radian_normalize(x);

    float euclid_coor_arr[3] = {1.0f, 1.0f, 1.0f};
    float radian_coor_arr[3]{};

    taylor_matrix::cuda_matrix::space_operation::euclidean_to_radian_coordinate(euclid_coor_arr, 3, radian_coor_arr);
}

int main()
{
    test_radian_normalize<<<1, 1>>>();
    test_restrict_scalar_mul_array<<<1, 1>>>();
    test_restrict_scalar_div_array<<<1, 1>>>();
    test_restrict_add_array<<<1, 1>>>();
    test_restrict_multidimensional_oval_to_euclidean_array<<<1, 1>>>();
    test_dot_product<<<1, 1>>>();
    test_coordinate_distance<<<1, 1>>>();
    test_cosine_score<<<1, 1>>>();
    test_cosine_angle<<<1, 1>>>();
    test_euclidean_to_radian_coordinate<<<1, 1>>>();
    test_radian_normalize_and_euclidean_to_radian_coordinate<<<1, 1>>>();

    cudaDeviceSynchronize();

    return 0;
}