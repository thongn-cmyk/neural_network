#ifndef __MEMORY_SYNCHRONIZER_FACTORY_H__
#define __MEMORY_SYNCHRONIZER_FACTORY_H__

#include <stdint.h>
#include <stdlib.h>
#include "host_memory_synchronizer.h"
#include "taint_interval_tree.h"
#include <exception>
#include <stdexcept>

namespace memory_sync
{
    class MemorySynchronizerFactory
    {
        public:

            static auto get_host_memory_synchronizer(const std::shared_ptr<MemoryReadableInterface>& memory_readable,
                                                     const std::vector<std::shared_ptr<AsyncMemoryWritableInterface>>& memory_writable_vec,
                                                     size_t memspan_sz) -> std::unique_ptr<MemorySynchronizerInterface>
            {
                if (memory_readable == nullptr)
                {
                    throw std::invalid_argument("bad memory readable argument, null");
                }

                for (const auto& memory_writable: memory_writable_vec)
                {
                    if (memory_writable == nullptr)
                    {
                        throw std::invalid_argument("bad memory writable, null");
                    }
                }

                return std::make_unique<HostMemorySynchronizer>(memory_readable,
                                                                memory_writable_vec,
                                                                std::make_unique<TaintIntervalTree>(memspan_sz));
            }
    };
}

#endif