#ifndef __MEMORY_SYNC_MEMORY_SYNCHRONIZER_INTERFACE_H__
#define __MEMORY_SYNC_MEMORY_SYNCHRONIZER_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include "memory_readable_interface.h"
#include "memory_writable_interface.h"

namespace memory_sync
{
    class MemorySynchronizerInterface
    {
        public:

            virtual ~MemorySynchronizerInterface() noexcept = default;

            virtual void taint_memory(const std::pair<size_t, size_t>& interval) = 0; 
            virtual void sync() = 0;
    };
}

#endif