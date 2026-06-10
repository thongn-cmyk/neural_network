#ifndef __MEMORY_SYNC_MEMORY_WRITEABLE_INTERFACE_H__
#define __MEMORY_SYNC_MEMORY_WRITEABLE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <concurrency_detachable_task/detachable_task_handle_interface.h>
#include <stl_extension/stdx.h>

namespace memory_sync
{
    class AsyncMemoryWritableInterface
    {
        public:

            using Promise   = std::shared_ptr<concurrency_detachable_task::DetachableTaskHandleInterface<stdx::fancy_void>>;

            virtual ~AsyncMemoryWritableInterface() noexcept = default;

            virtual auto write(size_t offset, size_t sz, const void * src) -> Promise = 0;
    };
}

#endif