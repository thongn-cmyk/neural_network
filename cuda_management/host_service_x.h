#ifndef __CUDA_MANAGEMENT_CU_HOST_X_SERVICE_H__
#define __CUDA_MANAGEMENT_CU_HOST_X_SERVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <memory>
#include <cstring>
#include <string_view>
#include <exception>
#include <stdexcept>
#include "assert.h"
#include "host_service.h"
#include "local_exception.h"

namespace cuda_management::host_service_x
{
    using namespace cuda_management::local_exception;

    class PartialBumpAllocator
    {
        private:

            struct BumpAllocationBucket
            {
                std::shared_ptr<char[]> buf;
                size_t used_sz;
                size_t buf_sz;
            };

            std::optional<BumpAllocationBucket> bump_allocation_bucket;

            static inline constexpr size_t DEFAULT_BUMP_ALLOCATION_BUCKET_SZ    = size_t{1} << 16;
            static inline constexpr size_t DEFAULT_BUMP_ALLOCATION_THRESHOLD    = size_t{1} << 12;

            size_t bump_allocation_bucket_sz;
            size_t bump_allocation_threshold;

        public:

            inline PartialBumpAllocator(size_t bump_allocation_bucket_sz,
                                        size_t bump_allocation_threshold): bump_allocation_bucket_sz(bump_allocation_bucket_sz),
                                                                           bump_allocation_threshold(bump_allocation_threshold),
                                                                           bump_allocation_bucket(std::nullopt)
            {
                if (bump_allocation_bucket_sz < bump_allocation_threshold)
                {
                    throw std::invalid_argument("bad bump_allocation_bucket_sz, bump_allocation_bucket_sz must be greater or equal to bump_allocation_threshold");
                }
            }

            inline PartialBumpAllocator(): PartialBumpAllocator(DEFAULT_BUMP_ALLOCATION_BUCKET_SZ,
                                                                DEFAULT_BUMP_ALLOCATION_THRESHOLD){}

            inline PartialBumpAllocator(const PartialBumpAllocator&) = delete;
            inline PartialBumpAllocator& operator =(const PartialBumpAllocator&) = delete;

            inline auto allocate(size_t sz) -> std::shared_ptr<char[]>
            {
                if (sz > this->bump_allocation_threshold)
                {
                    return this->huge_allocate(sz);
                }
                else
                {
                    return this->small_allocate(sz);
                }
            }

        private:

            inline void make_bump_allocation_bucket()
            {
                this->bump_allocation_bucket = BumpAllocationBucket
                {
                    .buf        = cuda_management::host_service::make_cuda_buffer_from_size(this->bump_allocation_bucket_sz),
                    .used_sz    = size_t{0},
                    .buf_sz     = this->bump_allocation_bucket_sz
                };
            }

            inline auto huge_allocate(size_t sz) -> std::shared_ptr<char[]>
            {
                return cuda_management::host_service::make_cuda_buffer_from_size(sz);
            }

            inline auto small_allocate(size_t sz) -> std::shared_ptr<char[]>
            {
                if (sz == 0u)
                {
                    return nullptr;
                }

                if (sz > this->bump_allocation_threshold)
                {
                    std::abort();
                }

                if (!this->bump_allocation_bucket.has_value())
                {
                    this->make_bump_allocation_bucket();
                }

                size_t free_sz = this->bump_allocation_bucket->buf_sz - this->bump_allocation_bucket->used_sz;

                if (free_sz < sz)
                {
                    this->make_bump_allocation_bucket();
                }

                std::unique_ptr<std::shared_ptr<char[]>> immutable_wrapper = std::make_unique<std::shared_ptr<char[]>>(this->bump_allocation_bucket->buf);

                auto destructor = [buf_holder = std::move(immutable_wrapper)](char * memory)
                {
                    *buf_holder = nullptr; //UB-check (this is to hinder optimizations that could happen, I don't yet know how???)
                    (void) memory;
                };

                char * memory                           = std::next(this->bump_allocation_bucket->buf.get(), this->bump_allocation_bucket->used_sz);
                std::shared_ptr<char[]> rs              = std::unique_ptr<char[], decltype(destructor)>(memory, std::move(destructor));

                this->bump_allocation_bucket->used_sz   += sz;

                return rs;
            }
    };

    class CudaAllocator
    {
        public:

            inline auto allocate(size_t sz) -> std::shared_ptr<char[]>
            {
                return cuda_management::host_service::make_cuda_buffer_from_size(sz);
            }
    };

    template <class Allocator = CudaAllocator>
    inline auto make_cuda_buffer_from_size(size_t sz,
                                           Allocator&& allocator = Allocator()) -> std::shared_ptr<char[]>
    {
        return allocator.allocate(sz);
    }

    template <class Allocator = CudaAllocator>
    inline auto make_cuda_buffer_from_host_view(std::string_view host_view,
                                                Allocator&& allocator = Allocator()) -> std::shared_ptr<char[]>
    {
        std::shared_ptr<char[]> rs  = make_cuda_buffer_from_size(host_view.size(), allocator);
        cuda_management::host_service::memcpy_host_to_device(rs.get(), host_view.data(), host_view.size());

        return rs;
    }

    inline auto cuda_to_host_buffer(const std::shared_ptr<char[]>& cuda_buf, size_t cuda_buf_sz) -> std::shared_ptr<char[]>
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

        std::shared_ptr<char[]> rs  = std::make_unique<char[]>(cuda_buf_sz);
        cuda_management::host_service::memcpy_device_to_host(rs.get(), cuda_buf.get(), cuda_buf_sz);

        return rs;
    }

    template <class T, class ...Args,
              class Allocator,
              std::enable_if_t<std::is_arithmetic_v<T>, bool> = true> //iec559 + compliances
    inline auto make_cuda_object(Allocator&& allocator, Args&& ...args) -> std::shared_ptr<T>
    {
        static_assert(sizeof(T) != 0u);

        T obj                                   = T(std::forward<Args>(args)...);
        std::array<char, sizeof(T)> byte_rep    = std::bit_cast<std::array<char, sizeof(T)>>(obj);
        std::shared_ptr<char[]> cuda_buf        = make_cuda_buffer_from_host_view(std::string_view(byte_rep.data(), byte_rep.size()), allocator);

        //we guarantee void -> T is safe because this is cuda pointer, and we don't touch the pointing data here in __host__, the otherwise can't be guaranteed

        return std::static_pointer_cast<T>(std::static_pointer_cast<void>(cuda_buf));
    }

    template <class T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
    inline auto read_cuda_object(const std::shared_ptr<T>& obj) -> T
    {
        if (obj == nullptr)
        {
            throw std::invalid_argument("bad object, null");
        }

        //we don't do strict types here because it has to be from make_cuda_object
        //The constraint is now not mandatory, but it is mandatory in the make_cuda_object

        std::array<char, sizeof(T)> byte_rep    = {};
        std::shared_ptr<char[]> host_buf        = cuda_to_host_buffer(std::static_pointer_cast<char[]>(std::static_pointer_cast<void>(obj)), sizeof(T));

        std::memcpy(byte_rep.data(), host_buf.get(), sizeof(T));

        return std::bit_cast<T>(byte_rep);
    }
}

#endif