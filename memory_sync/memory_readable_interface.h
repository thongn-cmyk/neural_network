#ifndef __MEMORY_SYNC_MEMORY_READABLE_INTERFACE_H__
#define __MEMORY_SYNC_MEMORY_READABLE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <concurrency_detachable_task/detachable_task_handle_interface.h>
#include <stl_extension/stdx.h>

namespace memory_sync
{
    class AsyncMemoryReadableInterface
    {
        public:

            using Promise   = std::shared_ptr<concurrency_detachable_task::DetachableTaskHandleInterface<stdx::fancy_void>>;

            virtual ~AsyncMemoryReadableInterface() noexcept = default;

            virtual auto read(void * dst, size_t offset, size_t sz) -> Promise = 0;
    };

    class MemoryReadableInterface
    {
        public:

            virtual ~MemoryReadableInterface() noexcept = default;

            virtual void read(void * dst, size_t offset, size_t sz) = 0;
    };
}

#endif