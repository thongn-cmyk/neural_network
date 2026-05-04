#ifndef __CUDA_MANAGEMENT_CU_HOST_SERVICE_H__
#define __CUDA_MANAGEMENT_CU_HOST_SERVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <memory>
#include <cuda_runtime.h>
#include <cstring>
#include <string_view>
#include <exception>
#include <stdexcept>
#include "assert.h"

namespace cuda_management::host_service
{
    struct cuda_bad_alloc: std::bad_alloc{};
    
    struct cuda_corruption: std::runtime_error
    {
        cuda_corruption(): std::runtime_error("cuda corruption"){}
    };

    struct cuda_invalid_argument: std::invalid_argument
    {
        cuda_invalid_argument(const char * msg): std::invalid_argument(msg){}
    };

    struct cuda_runtime_error: std::runtime_error
    {
        cuda_runtime_error(const char * msg): std::runtime_error(msg){}
    };

    auto make_cuda_buffer_from_size(size_t sz) -> std::shared_ptr<char[]>
    {
        if (sz == 0u)
        {
            return nullptr;
        }

        void * cuda_buf = nullptr;
        cudaError_t err = cudaMalloc(static_cast<void **>(&cuda_buf), sz);

        if (err != cudaSuccess)
        {
            throw cuda_bad_alloc();
        }

        if (cuda_buf == nullptr)
        {
            throw cuda_corruption();
        }

        auto destructor = [](char * buf) noexcept
        {
            cudaFree(static_cast<void *>(buf));
        };

        return std::unique_ptr<char[], decltype(destructor)>(static_cast<char *>(cuda_buf), destructor);
    }

    auto make_cuda_buffer_from_host_view(std::string_view host_view) -> std::shared_ptr<char[]>
    {
        std::shared_ptr<char[]> rs  = make_cuda_buffer_from_size(host_view.size());

        if (host_view.size() == 0u)
        {
            return rs;
        }

        cudaError_t err             = cudaMemcpy(rs.get(), host_view.data(), host_view.size(), cudaMemcpyHostToDevice);

        if (err != cudaSuccess)
        {
            throw cuda_runtime_error(cudaGetErrorString(err));
        }

        return rs;
    }

    auto cuda_to_host_buf(const std::shared_ptr<char[]>& cuda_buf, size_t cuda_buf_sz) -> std::shared_ptr<char[]>
    {
        if (cuda_buf == nullptr)
        {
            if (cuda_buf_sz == 0u)
            {
                return nullptr;
            }
            else
            {
                throw cuda_invalid_argument("corrupted buffer");
            }
        }

        std::shared_ptr<char[]> rs  = std::shared_ptr<char[]>(new char[cuda_buf_sz]);
        cudaError_t err             = cudaMemcpy(rs.get(), cuda_buf.get(), cuda_buf_sz, cudaMemcpyDeviceToHost);

        if (err != cudaSuccess)
        {
            throw cuda_runtime_error(cudaGetErrorString(err));
        }

        return rs;
    }
}

#endif