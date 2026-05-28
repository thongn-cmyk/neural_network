#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <matrix_broker_server/starter.h>
#include <taylor_matrix/cuda_matrix/tensor_matrix_forward.h>
#include <taylor_matrix/cuda_matrix/tensor_matrix_forward_to_deviation.h>
#include <cuda_management/host_service.h>

int main()
{
    matrix_broker_server::start_server();
}