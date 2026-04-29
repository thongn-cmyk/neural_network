//HEADER_CONTROL 0

#ifndef __CU_X_H__
#define __CU_X_H__

#define DEVICE_IDENTIFIER __device__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <thread>
#include <memory>
#include <cuda_runtime.h>
#include <cstring>
#include <string_view>
#include <exception>
#include <stdexcept>
#include "assert.h"

namespace cu_x
{
    DEVICE_IDENTIFIER void panic_cuda_trap()
    {
        assert(false);
    }

    template <class T, std::enable_if_t<std::numeric_limits<T>::is_integer, bool> = true>
    static constexpr auto is_pow2(T value) noexcept -> bool
    {
        if (value == 0u)
        {
            return false;
        }

        T value_one = value - 1u; //godzilla
        return (value & value_one) == 0u;
    }

    template <uintptr_t ALIGNMENT_SZ>
    static constexpr auto align(uintptr_t ptr, const std::integral_constant<uintptr_t, ALIGNMENT_SZ>) noexcept -> uintptr_t
    {
        static_assert(is_pow2(ALIGNMENT_SZ));

        uintptr_t fwd_sz    = ALIGNMENT_SZ - 1u;
        uintptr_t bit_mask  = ~fwd_sz;
        uintptr_t fwd_ptr   = ptr + fwd_sz;

        return fwd_ptr & bit_mask;
    }

    static constexpr auto align(uintptr_t ptr, uintptr_t alignment_sz) noexcept -> uintptr_t
    {
        assert(is_pow2(alignment_sz));

        uintptr_t fwd_sz    = alignment_sz - 1u;
        uintptr_t bit_mask  = ~fwd_sz;
        uintptr_t fwd_ptr   = ptr + fwd_sz;

        return fwd_ptr & bit_mask;
    }   

    template <class T>
    __attribute__((noinline)) DEVICE_IDENTIFIER auto object_malloc(size_t object_count) -> char *
    {
        if (object_count == 0u)
        {
            return nullptr;
        }

        static_assert(sizeof(T) != 0u);
        static_assert(alignof(T) != 0u);

        constexpr size_t FWD_SZ     = alignof(T) - 1u;
        static_assert(FWD_SZ <= std::numeric_limits<uint16_t>::max());

        size_t sz                   = object_count * sizeof(T);
        size_t total_sz             = sz + sizeof(uint16_t);
        size_t aligned_total_sz     = total_sz + FWD_SZ;

        void * void_buf;
        cudaError_t  err            = cudaMalloc(static_cast<void **>(&void_buf), aligned_total_sz);

        if (err != cudaSuccess)
        {
            panic_cuda_trap();
        }

        if (void_buf == nullptr)
        {
            panic_cuda_trap();
        }

        char * buf                  = static_cast<char *>(void_buf);

        char * fwd_buf              = std::next(buf, sizeof(uint16_t));
        char * aligned_fwd_buf      = reinterpret_cast<char *>(align(reinterpret_cast<uintptr_t>(fwd_buf), std::integral_constant<uintptr_t, alignof(T)>{}));

        char * prev_aligned_fwd_buf = std::prev(aligned_fwd_buf, sizeof(uint16_t));
        uint16_t dist               = std::distance(fwd_buf, aligned_fwd_buf);

        std::memcpy(prev_aligned_fwd_buf, &dist, sizeof(uint16_t));

        return aligned_fwd_buf;
    }

    __attribute__((noinline)) DEVICE_IDENTIFIER void object_free(void * buf) noexcept
    {
        if (buf == nullptr)
        {
            return;
        }

        char * char_buf = static_cast<char *>(buf);
        char * prev_buf = std::prev(char_buf, sizeof(uint16_t));

        uint16_t dist{};
        std::memcpy(&dist, prev_buf, sizeof(uint16_t));

        char * fwd_buf  = std::prev(char_buf, dist);
        char * org_buf  = std::prev(fwd_buf, sizeof(uint16_t));

        cudaFree(org_buf);
    }

    template <class T>
    class CudaSTLAllocator
    {
        private:

            template <class U>
            friend class CudaSTLAllocator;

            using self              = CudaSTLAllocator;

        public:

            using value_type        = T;
            using pointer           = T*;
            using const_pointer     = const T*;
            using reference         = T&;
            using const_reference   = const T&;
            using size_type         = std::size_t;
            using difference_type   = std::ptrdiff_t;

            template <class U>
            struct rebind
            {
                using other = CudaSTLAllocator<U>;
            };

            template <class U, std::enable_if_t<std::negation_v<std::is_same<T, U>>, bool> = true>
            DEVICE_IDENTIFIER CudaSTLAllocator(const CudaSTLAllocator<U>&) noexcept{}

            DEVICE_IDENTIFIER auto allocate(size_t n) -> T *
            {
                return static_cast<T *>(static_cast<void *>(object_malloc<T>(n)));
            }

            DEVICE_IDENTIFIER void deallocate(T * ptr, size_t sz)
            {
                object_free(ptr);
            }

            template <class ...Args>
            DEVICE_IDENTIFIER void construct(T * ptr, Args&& ...args)
            {
                new (ptr) T(std::forward<Args>(args)...);
            }

            DEVICE_IDENTIFIER void destroy(T * ptr)
            {
                std::destroy_at(ptr);
            }

            DEVICE_IDENTIFIER auto operator ==(const self& rhs) const noexcept -> bool
            {
                return true;
            }

            DEVICE_IDENTIFIER auto operator !=(const self& rhs) const noexcept -> bool
            {
                return false;
            }
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
            throw std::bad_alloc();
        }

        if (cuda_buf == nullptr)
        {
            throw std::runtime_error("cuda corruption");
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
            throw std::runtime_error(cudaGetErrorString(err));
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
                throw std::invalid_argument("corrupted buffer");
            }
        }

        std::shared_ptr<char[]> rs  = std::shared_ptr<char[]>(new char[cuda_buf_sz]);
        cudaError_t err             = cudaMemcpy(rs.get(), cuda_buf.get(), cuda_buf_sz, cudaMemcpyDeviceToHost);

        if (err != cudaSuccess)
        {
            throw std::runtime_error(cudaGetErrorString(err));
        }

        return rs;
    }
}

#endif