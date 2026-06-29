//HEADER_CONTROL 0

#ifndef __CUDA_MANAGEMENT_DEVICE_MEMORY_H__
#define __CUDA_MANAGEMENT_DEVICE_MEMORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <cuda_runtime.h>
#include "assert.h"
#include "utility.h"
#include <serializer/trivial_serializer.h>
#include <cuda/std/limits>
#include <global_config/cuda_device_memory_config.h>
#include <type_traits>

namespace cuda_management::device_memory
{
    class HeapAllocator
    {
        public:

            __device__ inline auto malloc(size_t sz) -> std::add_pointer_t<void>
            {
                if (sz == 0u)
                {
                    return nullptr;
                }

                void * rs = new char[sz];

                if (rs == nullptr)
                {
                    assert(false);
                }

                return rs;
            }

            __device__ inline void free(void * buf) noexcept
            {
                if (buf == nullptr)
                {
                    return;
                }

                delete[] static_cast<char *>(buf);
            }
    };

    __device__ static constexpr inline uint32_t ON_DEVICE_MEMORY_SZ     = global_config::cuda_device_memory_config::ON_DEVICE_MEMORY_SZ;
    __device__ static constexpr inline bool USE_SESSION_MEMORY          = global_config::cuda_device_memory_config::USE_SESSION_MEMORY;
 
    __device__ char MEM_POOL[ON_DEVICE_MEMORY_SZ];

    __device__ uint32_t bump_ptr            = 0u;
    __device__ uint32_t in_use_memory_sz    = 0u;

    //I would not let myself to not implement a bound check, because it is so mandatory that I don't think that we can get away with it
    //let just keep the must-implement safety feature here for backlog, because it is in the std::abort() category, we can get away with production if careful, but we are not careful  

    //this is different from normal errors of malloc() -> free(), check buffers, it's skill issues, it's strictly in the std::abort() category and we can disable it in production
    //this is some high-risk errors that need safeguard to std::abort()

    class SessionAllocator
    {
        public:

            __device__ inline auto malloc(size_t sz) -> std::add_pointer_t<void>
            {
                if (sz == 0u)
                {
                    return nullptr;
                }

                size_t first    = atomicAdd(&bump_ptr, sz) % ON_DEVICE_MEMORY_SZ; 
                size_t last     = first + sz;

                if (last > ON_DEVICE_MEMORY_SZ)
                {
                    first   = atomicAdd(&bump_ptr, sz) % ON_DEVICE_MEMORY_SZ;
                    last    = first + sz;

                    if (last > ON_DEVICE_MEMORY_SZ)
                    {
                        assert(false);
                    }
                }

                return MEM_POOL + first;
            }

            __device__ inline void free(void * buf) noexcept
            {
                (void) buf;
            }
    };

    using CudaAllocator = std::conditional_t<USE_SESSION_MEMORY,
                                             SessionAllocator,
                                             HeapAllocator>;

    __device__ static inline constexpr size_t DEFAULT_ALIGNMENT_SZ          = 1u;
    __device__ static inline constexpr size_t DEFAULT_OBJECT_ALIGNMENT_SZ   = alignof(std::max_align_t); 

    using alignment_header_t        = uint32_t;
    using xalign_metadata_size_t    = uint32_t;

    template <class T>
    __device__ static constexpr auto cuda_align_of() -> size_t
    {
        return utility::max(alignof(T), DEFAULT_OBJECT_ALIGNMENT_SZ);
    }

    __device__ static constexpr auto dg_align(void * buf,
                                              uintptr_t alignment_sz) noexcept -> void *
    {
        assert(utility::is_pow2(alignment_sz));

        uintptr_t arithmetic_buf        = reinterpret_cast<uintptr_t>(buf);
        uintptr_t FWD_SZ                = alignment_sz - 1u;
        uintptr_t MASK_VALUE            = ~FWD_SZ;
        uintptr_t fwd_arithmetic_buf    = (arithmetic_buf + FWD_SZ) & MASK_VALUE;

        return reinterpret_cast<void *>(fwd_arithmetic_buf);
    }

    __device__ static constexpr auto dg_align(const void * buf,
                                              uintptr_t alignment_sz) noexcept -> const void *
    {
        assert(utility::is_pow2(alignment_sz));

        uintptr_t arithmetic_buf        = reinterpret_cast<uintptr_t>(buf);
        uintptr_t FWD_SZ                = alignment_sz - 1u;
        uintptr_t MASK_VALUE            = ~FWD_SZ;
        uintptr_t fwd_arithmetic_buf    = (arithmetic_buf + FWD_SZ) & MASK_VALUE;

        return reinterpret_cast<const void *>(fwd_arithmetic_buf);
    }

    template <class AllocatorInterface>
    __device__ inline auto dg_malloc(size_t blk_sz,
                                     AllocatorInterface&& allocator) -> void *
    {
        return allocator.malloc(blk_sz); 
    }

    template <class AllocatorInterface>
    __device__ inline void dg_free(void * ptr,
                                   AllocatorInterface&& allocator) noexcept
    {
        allocator.free(ptr);
    }

    template <class AllocatorInterface>
    __device__ inline __attribute__((noinline, noipa)) void obj_dg_free(void * ptr,
                                                                        AllocatorInterface&& allocator) noexcept
    {
        dg_free(ptr, allocator);
    }

    template <class AllocatorInterface>
    __device__ inline auto dg_aligned_alloc(size_t alignment, size_t blk_sz,
                                            AllocatorInterface&& allocator) -> void *
    {
        if (!utility::is_pow2(alignment))
        {
            assert(false);
        }

        const size_t max_fwd_sz = alignment + (sizeof(alignment_header_t) - 1u);

        if (max_fwd_sz > cuda::std::numeric_limits<alignment_header_t>::max())
        {
            assert(false);
        }

        if (blk_sz == 0u)
        {
            return nullptr;
        }

        size_t adj_blk_sz   = blk_sz + max_fwd_sz;
        void * ptr          = allocator.malloc(adj_blk_sz);

        if (ptr == nullptr)
        {
            assert(false);
        }

        void * aligned_ptr              = dg_align(utility::next(static_cast<char *>(ptr), sizeof(alignment_header_t)), alignment);
        alignment_header_t difference   = utility::distance(static_cast<char *>(ptr), static_cast<char *>(aligned_ptr));
        void * alignment_header_addr    = utility::prev(static_cast<char *>(aligned_ptr), sizeof(alignment_header_t));

        std::memcpy(alignment_header_addr, &difference, sizeof(alignment_header_t));

        return aligned_ptr; 
    } 

    template <class AllocatorInterface>
    __device__ inline void dg_aligned_free(void * ptr,
                                           AllocatorInterface&& allocator) noexcept
    {
        if (ptr == nullptr)
        {
            return;
        }

        void * alignment_header_addr    = utility::prev(static_cast<char *>(ptr), sizeof(alignment_header_t));
        alignment_header_t difference;
        std::memcpy(&difference, alignment_header_addr, sizeof(alignment_header_t));
        void * org_ptr                  = utility::prev(static_cast<char *>(ptr), difference); 

        allocator.free(org_ptr); 
    }

    template <class AllocatorInterface>
    __device__ inline __attribute__((noinline, noipa)) void obj_dg_aligned_free(void * ptr,
                                                                                AllocatorInterface&& allocator) noexcept
    {
        dg_aligned_free(ptr, allocator);
    }

    struct XAlignMetadata
    {
        alignment_header_t difference;
        xalign_metadata_size_t blk_sz; 

        template <class Reflector>
        __device__ constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(difference, blk_sz);
        }

        template <class Reflector>
        __device__ constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(difference, blk_sz);
        }
    };

    template <class AllocatorInterface>
    __device__ inline auto dg_xaligned_alloc(size_t alignment, size_t blk_sz,
                                             AllocatorInterface&& allocator) -> void *
    {
        constexpr size_t METADATA_SZ = trivial_serializer::size(XAlignMetadata{});

        if (!utility::is_pow2(alignment))
        {
            assert(false);
        }

        const size_t max_fwd_sz = alignment + (METADATA_SZ - 1u);

        if (max_fwd_sz > cuda::std::numeric_limits<alignment_header_t>::max())
        {
            assert(false);
        }

        if (blk_sz == 0u)
        {
            return nullptr;
        }

        size_t adj_blk_sz   = blk_sz + max_fwd_sz;
        void * ptr          = allocator.malloc(adj_blk_sz);

        if (ptr == nullptr)
        {
            assert(false);
        }

        void * aligned_ptr              = dg_align(utility::next(static_cast<char *>(ptr), METADATA_SZ), alignment); //forward METADATA_SZ to reserve the METADATA_SZ, align the alignment (guaranteed to fit because we have extra ALIGMENT_SZ - 1u)
        alignment_header_t difference   = utility::distance(static_cast<char *>(ptr), static_cast<char *>(aligned_ptr));
        void * metadata_header_addr     = utility::prev(static_cast<char *>(aligned_ptr), METADATA_SZ);

        trivial_serializer::serialize_into(static_cast<char *>(metadata_header_addr), XAlignMetadata{.difference    = difference, 
                                                                                                     .blk_sz        = utility::wrap_safe_integer_cast(blk_sz)});

        return aligned_ptr;
    }

    template <class AllocatorInterface>
    __device__ inline void dg_xaligned_free(void * ptr,
                                            AllocatorInterface&& allocator) noexcept
    {
        if (ptr == nullptr)
        {
            return;
        }

        constexpr size_t METADATA_SZ    = trivial_serializer::size(XAlignMetadata{});

        void * metadata_header_addr     = utility::prev(static_cast<char *>(ptr), METADATA_SZ);
        auto metadata                   = XAlignMetadata{};
        trivial_serializer::deserialize_into(metadata, static_cast<const char *>(metadata_header_addr));
        void * org_ptr                  = utility::prev(static_cast<char *>(ptr), metadata.difference); 

        allocator.free(org_ptr); 
    }

    template <class AllocatorInterface>
    __device__ inline __attribute__((noinline, noipa)) void obj_dg_xaligned_free(void * ptr,
                                                                                 AllocatorInterface&& allocator) noexcept
    {
        dg_xaligned_free(ptr, allocator);
    }

    __device__ inline auto dg_xaligned_blk_size(const void * ptr) noexcept -> size_t
    {
        if (ptr == nullptr)
        {
            return 0u;
        }

        constexpr size_t METADATA_SZ        = trivial_serializer::size(XAlignMetadata{});

        const void * metadata_header_addr   = utility::prev(static_cast<const char *>(ptr), METADATA_SZ);
        auto metadata                       = XAlignMetadata{};

        trivial_serializer::deserialize_into(metadata, static_cast<const char *>(metadata_header_addr));

        return metadata.blk_sz;
    }

    template <class T, class ...Args, class AllocatorInterface>
    __device__ inline auto std_new_object(AllocatorInterface&& allocator, Args&& ...args) -> T *
    {
        static_assert(sizeof(T) != 0u);
        void * blk = nullptr;

        if constexpr(cuda_align_of<T>() <= DEFAULT_ALIGNMENT_SZ)
        {
            blk = dg_malloc(sizeof(T), allocator);
        }
        else
        {
            blk = dg_aligned_alloc(cuda_align_of<T>(), sizeof(T), allocator);
        }

        if (blk == nullptr)
        {
            assert(false);
        }

        return new (blk) T(std::forward<Args>(args)...);
    }

    template <class = void>
    __device__ static inline constexpr bool FALSE_VAL = false;

    template <class T, class AllocatorInterface>
    __device__ inline auto std_delete_object(AllocatorInterface&& allocator, T * obj) noexcept
    {
        utility::destroy_at(obj);

        if constexpr(cuda_align_of<T>() <= DEFAULT_ALIGNMENT_SZ)
        {
            obj_dg_free(static_cast<void *>(obj), allocator);
        }
        else
        {
            obj_dg_aligned_free(static_cast<void *>(obj), allocator);
        }
    }

    template <class T, class AllocatorInterface>
    __device__ inline auto std_new_array(AllocatorInterface&& allocator, size_t sz) -> T *
    {
        static_assert(sizeof(T) != 0u);

        if (sz == 0u)
        {
            return nullptr;
        }

        size_t allocation_blk_sz    = sz * sizeof(T); 
        void * blk                  = dg_xaligned_alloc(cuda_align_of<T>(), allocation_blk_sz, allocator);

        if (blk == nullptr)
        {
            assert(false);
        }  

        return new (blk) T[sz];
    }

    template <class T, class AllocatorInterface>
    __device__ inline void std_delete_array(AllocatorInterface&& allocator, T * arr) noexcept
    {
        if (arr == nullptr)
        {
            return;
        }

        size_t allocation_blk_sz    = dg_xaligned_blk_size(arr);
        size_t sz                   = allocation_blk_sz / sizeof(T);

        utility::destroy(arr, utility::next(arr, sz));
        obj_dg_xaligned_free(static_cast<void *>(arr), allocator);
    }
}

#endif