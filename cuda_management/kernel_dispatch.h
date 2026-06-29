#ifndef __CUDA_MANAGEMENT_KERNEL_DISPATCH_H__
#define __CUDA_MANAGEMENT_KERNEL_DISPATCH_H__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <stl_extension/stdx.h>
#include <semaphore>

namespace cuda_management::kernel_dispatch
{
    struct SmpSignature{};

    static inline constexpr size_t KERNEL_CONCURRENT_DISPATCH_COUNT = size_t{1} << 0;

    using semaphore             = std::counting_semaphore<KERNEL_CONCURRENT_DISPATCH_COUNT>; 
    using SmpSingletonContainer = stdx::singleton_container<std::unique_ptr<semaphore>, SmpSignature>;

    inline void init()
    {
        stdx::memtransaction_guard tx_grd;

        SmpSingletonContainer::get() = std::make_unique<semaphore>(KERNEL_CONCURRENT_DISPATCH_COUNT);
    }

    inline void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        SmpSingletonContainer::get() = nullptr;
    }

    template <class KernelFunction>
    inline auto get_block_thread(KernelFunction func,
                                 size_t concurrent_dispatch_sz) -> std::pair<size_t, size_t>
    {
        using namespace local_exception;

        int optimal_blk_sz{};
        int min_grid_sz{};

        cudaError_t err = cudaOccupancyMaxPotentialBlockSize
        (
            &min_grid_sz,
            &optimal_blk_sz,
            func,
            0,
            0
        );

        if (err != cudaSuccess)
        {
            throw cuda_invalid_argument(cudaGetErrorString(err));
        }

        if (optimal_blk_sz <= 0)
        {
            throw cuda_invalid_argument("bad invoke, unable to get cuda metadata");
        }

        const size_t MAX_THREAD_SZ  = optimal_blk_sz;

        if (concurrent_dispatch_sz == 0u)
        {
            return std::make_pair(size_t{0u}, size_t{0u});
        }

        if (concurrent_dispatch_sz < MAX_THREAD_SZ)
        {
            return std::make_pair(size_t{1}, concurrent_dispatch_sz);
        }

        size_t ceil_dispatch_sz = stdx::mul_ceil(concurrent_dispatch_sz, MAX_THREAD_SZ);
        size_t blk_sz           = ceil_dispatch_sz / MAX_THREAD_SZ;

        return std::make_pair(blk_sz, MAX_THREAD_SZ);
    }

    inline auto get_semaphore() -> semaphore&
    {
        if (SmpSingletonContainer::get() == nullptr)
        {
            std::abort();
        }

        return *SmpSingletonContainer::get();
    }
}

#endif