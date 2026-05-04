#include <taylor_matrix/cuda_matrix/tensor_process_unit_operation.h>

using namespace taylor_matrix::cuda_matrix::tensor_model;
using namespace taylor_matrix::cuda_matrix::utility;

void test_two_to_one_project()
{
    size_t sz{};

    taylor_matrix::cuda_matrix::tensor_process_unit_operation::two_to_one_project(ProcessUnit{}, ProcessUnit{},
                                                                                  to_size_container(std::integral_constant<size_t, 2>{}),
                                                                                  {}, sz, {});
}

void test_batch_two_to_one_project()
{
    size_t sz{};

    taylor_matrix::cuda_matrix::tensor_process_unit_operation::batch_two_to_one_project({}, {}, std::integral_constant<size_t, 2u>{}, {},
                                                                                        to_size_container(std::integral_constant<size_t, 1u>{}),
                                                                                        {}, sz, {});
}

void test_deparameterize()
{

}

void test_accumulate()
{

}

void test_positional_encode()
{

}

void test_div()
{

}

void test_avg()
{

}

int main()
{
    test_two_to_one_project();
    test_batch_two_to_one_project();
    test_deparameterize();
    test_accumulate();
    test_positional_encode();
    test_div();
    test_avg();

    return 0;
}