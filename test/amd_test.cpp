#include <hip/hip_runtime.h>
#include <iostream>
#include <vector>

// HIP Kernel to add two vectors
__global__ void vectorAdd(const float* A, const float* B, float* C, int N) {
    int i = hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x;
    if (i < N) {
        C[i] = A[i] + B[i];
    }
}

// Standard macro for checking HIP API calls
#define HIP_CHECK(expression) {                           \
    const hipError_t status = expression;                 \
    if (status != hipSuccess) {                           \
        std::cerr << "HIP error " << status << ": "       \
                  << hipGetErrorString(status) << " at "  \
                  << __FILE__ << ":" << __LINE__          \
                  << std::endl;                           \
        exit(1);                                          \
    }                                                     \
}

int main() {
    int N = 1000000;
    size_t size = N * sizeof(float);

    // 1. Allocate Host Memory
    std::vector<float> h_A(N, 1.0f), h_B(N, 2.0f), h_C(N);

    // 2. Allocate Device Memory
    float *d_A, *d_B, *d_C;
    
    HIP_CHECK(hipMalloc(&d_A, size));
    HIP_CHECK(hipMalloc(&d_B, size));
    HIP_CHECK(hipMalloc(&d_C, size));

    // 3. Copy Data from Host to Device
    HIP_CHECK(hipMemcpy(d_A, h_A.data(), size, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_B, h_B.data(), size, hipMemcpyHostToDevice));

    // 4. Launch Kernel
    int threadsPerBlock = 256;
    int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;

    hipLaunchKernelGGL(vectorAdd, dim3(blocksPerGrid), dim3(threadsPerBlock), 0, 0, d_A, d_B, d_C, N);
    HIP_CHECK(hipGetLastError());

    // 5. Copy Data back to Host
    hipMemcpy(h_C.data(), d_C, size, hipMemcpyDeviceToHost);

    // 6. Free Device Memory
    hipFree(d_A);
    hipFree(d_B);
    hipFree(d_C);

    std::cout << "Success: " << h_C[0] << std::endl; // Should be 3.0
    return 0;
}