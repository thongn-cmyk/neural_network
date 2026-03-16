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

#endif
