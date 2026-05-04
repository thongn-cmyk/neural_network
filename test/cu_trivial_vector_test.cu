
#include <cuda_management/cuda_vector.h>

template <class T>
using cuda_vector = cuda_management::cuda_vector::trivial_cuda_vector<T>;

__global__ void test_vector_init_empty()
{
    cuda_vector<int> v;
    assert(v.size() == 0);
    assert(v.capacity() == 0);
    assert(v.empty() == true);
}

__global__ void test_vector_init_by_constant()
{
    cuda_vector<int> v(5, 42);
    assert(v.size() == 5);
    assert(v[0] == 42);
    assert(v[4] == 42);
}

__global__ void test_vector_init_by_iterator()
{
    int arr[] = {1, 2, 3, 4, 5};
    cuda_vector<int> v(arr, arr + 5);
    assert(v.size() == 5);
    assert(v[0] == 1);
    assert(v[4] == 5);
}

__global__ void test_vector_init_by_copy()
{
    cuda_vector<int> v1(3, 10);
    cuda_vector<int> v2(v1);
    assert(v2.size() == 3);
    assert(v2[0] == 10);
}

__global__ void test_vector_init_by_move()
{
    cuda_vector<int> v1(3, 20);
    cuda_vector<int> v2(cuda_vector<int>(3, 20));
    assert(v2.size() == 3);
    assert(v2[0] == 20);
}

__global__ void test_vector_assign_by_copy()
{
    cuda_vector<int> v1(3, 5);
    cuda_vector<int> v2;
    v2 = v1;
    assert(v2.size() == 3);
    assert(v2[0] == 5);
}

__global__ void test_vector_assign_by_move()
{
    cuda_vector<int> v1;
    v1 = cuda_vector<int>(4, 15);
    assert(v1.size() == 4);
    assert(v1[0] == 15);
}

__global__ void test_vector_swap()
{
    cuda_vector<int> v1(2, 1);
    cuda_vector<int> v2(3, 2);
    v1.swap(v2);
    assert(v1.size() == 3);
    assert(v1[0] == 2);
}

__global__ void test_vector_reserve()
{
    cuda_vector<int> v;
    v.reserve(10);
    assert(v.capacity() >= 10);
    assert(v.size() == 0);
}

__global__ void test_vector_resize()
{
    cuda_vector<int> v(3, 5);
    v.resize(5, 0);
    assert(v.size() == 5);
    assert(v[4] == 0);
}

__global__ void test_vector_push_back()
{
    cuda_vector<int> v;
    v.push_back(1);
    v.push_back(2);
    assert(v.size() == 2);
    assert(v[1] == 2);
}

__global__ void test_vector_emplace_back()
{
    cuda_vector<int> v;
    v.emplace_back(10);
    v.emplace_back(20);
    assert(v.size() == 2);
    assert(v.back() == 20);
}

__global__ void test_vector_pop_back()
{
    cuda_vector<int> v(3, 5);
    v.pop_back();
    assert(v.size() == 2);
}

__global__ void test_vector_clear()
{
    cuda_vector<int> v(5, 3);
    v.clear();
    assert(v.size() == 0);
    assert(v.empty() == true);
}

__global__ void test_vector_front()
{
    cuda_vector<int> v = {1, 2, 3};
    assert(v.front() == 1);
}

__global__ void test_vector_back()
{
    cuda_vector<int> v = {1, 2, 3};
    assert(v.back() == 3);
}

__global__ void test_vector_begin()
{
    cuda_vector<int> v = {5, 6, 7};
    assert(*v.begin() == 5);
}

__global__ void test_vector_cbegin()
{
    cuda_vector<int> v = {5, 6, 7};
    assert(*v.cbegin() == 5);
}

__global__ void test_vector_end()
{
    cuda_vector<int> v = {1, 2, 3};
    assert(*(v.end() - 1) == 3);
}

__global__ void test_vector_cend()
{
    cuda_vector<int> v = {1, 2, 3};
    assert(*(v.cend() - 1) == 3);
}

__global__ void test_vector_at()
{
    cuda_vector<int> v = {10, 20, 30};
    assert(v.at(1) == 20);
}

__global__ void test_vector_bracket()
{
    cuda_vector<int> v = {100, 200, 300};
    assert(v[2] == 300);
}

__global__ void test_vector_size()
{
    cuda_vector<int> v(7, 0);
    assert(v.size() == 7);
}

__global__ void test_vector_capacity()
{
    cuda_vector<int> v;
    v.reserve(20);
    assert(v.capacity() >= 20);
}

__global__ void test_vector_empty()
{
    cuda_vector<int> v;
    assert(v.empty() == true);
    v.push_back(1);
    assert(v.empty() == false);
}

__global__ void test_vector_data()
{
    cuda_vector<int> v = {1, 2, 3};
    int* ptr = v.data();
    assert(ptr[0] == 1);
}

__global__ void test_vector_less()
{
    cuda_vector<int> v1 = {1, 2};
    cuda_vector<int> v2 = {1, 3};
    assert(v1 < v2);
}

__global__ void test_vector_ge()
{
    cuda_vector<int> v1 = {1, 3};
    cuda_vector<int> v2 = {1, 2};
    assert(v1 >= v2);
}

__global__ void test_vector_greater()
{
    cuda_vector<int> v1 = {2, 1};
    cuda_vector<int> v2 = {1, 5};
    assert(v1 > v2);
}

__global__ void test_vector_le()
{
    cuda_vector<int> v1 = {1, 2};
    cuda_vector<int> v2 = {1, 3};
    assert(v1 <= v2);
}

__global__ void test_vector_eq()
{
    cuda_vector<int> v1 = {1, 2, 3};
    cuda_vector<int> v2 = {1, 2, 3};
    assert(v1 == v2);
}

__global__ void test_vector_not_equal()
{
    cuda_vector<int> v1 = {1, 2};
    cuda_vector<int> v2 = {1, 3};
    assert(v1 != v2);
}

int main()
{
    test_vector_init_empty<<<1, 1>>>();
    test_vector_init_by_constant<<<1, 1>>>();
    test_vector_init_by_iterator<<<1, 1>>>();
    test_vector_init_by_copy<<<1, 1>>>();
    test_vector_init_by_move<<<1, 1>>>();
    test_vector_assign_by_copy<<<1, 1>>>();
    test_vector_assign_by_move<<<1, 1>>>();
    test_vector_swap<<<1, 1>>>();
    test_vector_reserve<<<1, 1>>>();
    test_vector_resize<<<1, 1>>>();
    test_vector_push_back<<<1, 1>>>();
    test_vector_emplace_back<<<1, 1>>>();
    test_vector_pop_back<<<1, 1>>>();
    test_vector_clear<<<1, 1>>>();
    test_vector_front<<<1, 1>>>();
    test_vector_back<<<1, 1>>>();
    test_vector_begin<<<1, 1>>>();
    test_vector_cbegin<<<1, 1>>>();
    test_vector_end<<<1, 1>>>();
    test_vector_cend<<<1, 1>>>();
    test_vector_at<<<1, 1>>>();
    test_vector_bracket<<<1, 1>>>();
    test_vector_size<<<1, 1>>>();
    test_vector_capacity<<<1, 1>>>();
    test_vector_empty<<<1, 1>>>();
    test_vector_data<<<1, 1>>>();
    test_vector_less<<<1, 1>>>();
    test_vector_ge<<<1, 1>>>();
    test_vector_greater<<<1, 1>>>();
    test_vector_le<<<1, 1>>>();
    test_vector_eq<<<1, 1>>>();
    test_vector_not_equal<<<1, 1>>>();

    cudaDeviceSynchronize();

    return 0;
}