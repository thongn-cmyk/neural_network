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

    using SmpSingletonContainer = stdx::singleton_container<std::unique_ptr<std::semaphore>, SmpSignature>;

    static inline constexpr size_t KERNEL_CONCURRENT_DISPATCH_COUNT = size_t{1} << 6;

    void init()
    {
        stdx::memtransaction_guard tx_grd;

        SmpSingletonContainer::get() = std::make_unique<std::semaphore>(KERNEL_CONCURRENT_DISPATCH_COUNT);
    }

    void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        SmpSingletonContainer::get() = nullptr;
    }

    auto get_block_thread(size_t concurrent_dispatch_sz) -> std::pair<size_t, size_t>
    {
        return {};
    }

    auto get_semaphore() -> std::semaphore&
    {
        if (SmpSingletonContainer::get() == nullptr)
        {
            std::abort();
        }

        return *SmpSingletonContainer::get();
    }
}

#endif