#ifndef __HOST_MEMORY_SYNCHRONIZER_H__
#define __HOST_MEMORY_SYNCHRONIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include "memory_synchronizer_interface.h"
#include "taint_interval_tree.h"

namespace memory_sync
{
    class HostMemorySynchronizer: public virtual MemorySynchronizerInterface
    {
        private:

            std::shared_ptr<MemoryReadableInterface> memory_readable;
            std::vector<std::shared_ptr<AsyncMemoryWritableInterface>> memory_writable_vec;
            std::unique_ptr<TaintIntervalTree> mem_interval_tree;

        public:

            HostMemorySynchronizer(std::shared_ptr<MemoryReadableInterface> memory_readable_arg,
                                   std::vector<std::shared_ptr<AsyncMemoryWritableInterface>> memory_writable_vec_arg,
                                   std::unique_ptr<TaintIntervalTree> mem_interval_tree_arg) noexcept: memory_readable(std::move(memory_readable_arg)),
                                                                                                       memory_writable_vec(std::move(memory_writable_vec_arg)),
                                                                                                       mem_interval_tree(std::move(mem_interval_tree_arg)){}

            void taint_memory(const std::pair<size_t, size_t>& interval)
            {
                this->mem_interval_tree->taint(interval);
            }

            void sync()
            {
                using Promise = AsyncMemoryWritableInterface::Promise;

                std::vector<std::pair<size_t, size_t>> interval_vec = this->mem_interval_tree->get_taint_region_vector();
                std::vector<Promise> promise_vec{};

                for (const auto& [offset, sz]: interval_vec)
                {
                    std::string byte_stream(sz, ' ');
                    this->memory_readable->read(byte_stream.data(), offset, sz);

                    for (const auto& dst: this->memory_writable_vec)
                    {
                        promise_vec.push_back(dst->write(offset, sz, byte_stream.data()));
                    }
                }

                for (const auto& promise: promise_vec)
                {
                    promise->wait();
                }

                this->mem_interval_tree->reset();
            }
    };
}

#endif