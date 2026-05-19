#ifndef __CUDA_MANAGEMENT_CU_HOST_SERVICE_H__
#define __CUDA_MANAGEMENT_CU_HOST_SERVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <memory>

#ifdef __CUDACC__

#include <cuda_runtime.h> //
#include "cuda_malloc.h"

#endif

#include <cstring>
#include <string_view>
#include <exception>
#include <stdexcept>
#include "assert.h"
#include "local_exception.h"

namespace cuda_management::host_service
{
    using namespace cuda_management::local_exception;

    extern auto make_cuda_buffer_from_size(size_t sz) -> std::shared_ptr<char[]>
    {
        #ifdef __CUDACC__
        {
            auto destructor = [](char * mem) noexcept
            {
                cuda_management::cuda_malloc::free(mem);
            };
            char * mem      = static_cast<char *>(cuda_management::cuda_malloc::malloc(sz));

            return std::unique_ptr<char[], decltype(destructor)>(mem, std::move(destructor));
        }
        #else
        {
            throw device_not_available_error("cuda device not supported");
        }
        #endif
    }

    extern void memcpy_host_to_device(void * dst, const void * src, size_t sz)
    {
        #ifdef __CUDACC__
        {
            if (sz == 0u)
            {
                return;
            }

            cudaError_t err = cudaMemcpy(dst, src, sz, cudaMemcpyHostToDevice);

            if (err != cudaSuccess)
            {
                throw cuda_runtime_error(cudaGetErrorString(err));
            }
        }
        #else
        {
            throw device_not_available_error("cuda device not supported");
        }
        #endif
    }

    extern void memcpy_device_to_host(void * dst, const void * src, size_t sz)
    {
        #ifdef __CUDACC__
        {
            if (sz == 0u)
            {
                return;
            }

            cudaError_t err = cudaMemcpy(dst, src, sz, cudaMemcpyDeviceToHost);

            if (err != cudaSuccess)
            {
                throw cuda_runtime_error(cudaGetErrorString(err));
            }
        }
        #else
        {
            throw device_not_available_error("cuda device not supported");
        }
        #endif
    }

    extern auto make_cuda_buffer_from_host_view(std::string_view host_view) -> std::shared_ptr<char[]>
    {
        #ifdef __CUDACC__
        {
            std::shared_ptr<char[]> rs  = make_cuda_buffer_from_size(host_view.size());
            memcpy_host_to_device(rs.get(), host_view.data(), host_view.size());

            return rs;
        }
        #else
        {
            throw device_not_available_error("cuda device not supported");
        }
        #endif
    }

    extern auto cuda_to_host_buffer(const std::shared_ptr<char[]>& cuda_buf, size_t cuda_buf_sz) -> std::shared_ptr<char[]>
    {
        #ifdef __CUDACC__
        {
            if (cuda_buf == nullptr)
            {
                if (cuda_buf_sz == 0u)
                {
                    return nullptr;
                }
                else
                {
                    std::abort();
                }
            }
            else
            {
                if (cuda_buf_sz == 0u)
                {
                    std::abort();
                }
            }

            std::shared_ptr<char[]> rs  = std::unique_ptr<char[]>(new char[cuda_buf_sz]);
            memcpy_device_to_host(rs.get(), cuda_buf.get(), cuda_buf_sz);

            return rs;
        }
        #else
        {
            throw device_not_available_error("cuda device not supported");
        }
        #endif
    }
}

#endif