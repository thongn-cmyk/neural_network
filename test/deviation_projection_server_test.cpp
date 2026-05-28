#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <deviation_projection_server/starter.h>
#include <taylor_matrix/cuda_matrix/tensor_matrix_forward.h>
#include <taylor_matrix/cuda_matrix/tensor_matrix_forward_to_deviation.h>
#include <cuda_management/host_service.h>

int main()
{
    deviation_projection_server::start_server();
    deviation_projection_server::stop_server();
}