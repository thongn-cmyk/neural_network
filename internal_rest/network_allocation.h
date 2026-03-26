#ifndef __DG_NETWORK_ALLOCATION_H__
#define __DG_NETWORK_ALLOCATION_H__

//define HEADER_CONTROL 4

#include <allocation_base/global_allocator.h>

namespace dg_sock::network_allocation
{
    using namespace allocation_base::global_allocator;
}

namespace dg_sock
{
    template <class T>
    using unique_ptr = allocation_base::global_allocator::unique_ptr<T>;

    template <class T>
    using shared_ptr = allocation_base::global_allocator::shared_ptr<T>;

    template <class T, class ...Args>
    auto make_unique(Args&& ...args) -> unique_ptr<T>
    {
        return allocation_base::global_allocator::make_unique<T>(std::forward<Args>(args)...);
    }

    template <class T, class ...Args>
    auto make_shared(Args&& ...args) -> shared_ptr<T>
    {
        return allocation_base::global_allocator::make_shared<T>(std::forward<Args>(args)...);
    }
}

// namespace dg_sock
// {
//     class CustomAllocator
//     {
//         public:

//             template <class T>
//             constexpr auto allocate_one() -> void *
//             {
//                 static_assert(sizeof(T) != 0u);

//                 return dg_sock::network_allocation::dg_aligned_alloc(alignof(T), sizeof(T));
//             }

//             constexpr void deallocate_one(void * memblk) noexcept
//             {
//                 dg_sock::network_allocation::dg_aligned_free(memblk);
//             }

//             template <class T>
//             constexpr auto allocate(size_t sz) -> void *
//             {
//                 static_assert(sizeof(T) != 0u);

//                 return dg_sock::network_allocation::dg_xaligned_alloc(alignof(T), sz * sizeof(T));
//             }

//             constexpr void deallocate(void * memblk) noexcept
//             {
//                 dg_sock::network_allocation::dg_xaligned_free(memblk);
//             }

//             constexpr auto size(const void * memblk) noexcept -> size_t
//             {
//                 return dg_sock::network_allocation::dg_xaligned_blk_size(memblk);
//             }
//     };

//     template <class T>
//     using unique_ptr = smart_pointer::unique_ptr_implementation::unique_ptr<T, CustomAllocator>;

//     template <class T>
//     using shared_ptr = smart_pointer::shared_ptr_implementation::shared_ptr<T, CustomAllocator>;

//     template <class T, class ...Args>
//     auto make_unique(Args&& ...args) -> unique_ptr<T>
//     {
//         return smart_pointer::unique_ptr_implementation::allocate_unique(CustomAllocator{}, std::forward<Args>(args)...);
//     }

//     template <class T, class ...Args>
//     auto make_shared(Args&& ...args) -> shared_ptr<T>
//     {
//         return smart_pointer::shared_ptr_implementation::allocate_shared(CustomAllocator{}, std::forward<Args>(args)...);
//     }
// }

#endif
