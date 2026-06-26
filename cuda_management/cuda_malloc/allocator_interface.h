#ifndef __CUDA_MANAGEMENT_CUDA_MALLOC_ALLOCATOR_INTERFACE_H__
#define __CUDA_MANAGEMENT_CUDA_MALLOC_ALLOCATOR_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

namespace cuda_mangement::cuda_malloc
{
    class AllocatorInterface
    {
        public:

            virtual ~AllocatorInterface() = default;

            virtual auto malloc(size_t sz) -> std::add_pointer_t<void> = 0;
            virtual void free(void * ptr) noexcept = 0;
    };
}

#endif