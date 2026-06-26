#ifndef __CUDA_MANAGEMENT_CUDA_MALLOC_NORMAL_ALLOCATOR_H__
#define __CUDA_MANAGEMENT_CUDA_MALLOC_NORMAL_ALLOCATOR_H__

#include <cuda_management/local_exception.h>
#include "allocator_interface.h"
#include <stdint.h>
#include <stdlib.h>
#include <cuda_runtime.h>

namespace cuda_mangement::cuda_malloc
{
    struct NormalAllocatorConfig
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    class NormalAllocator: public virtual AllocatorInterface
    {
        public:

            NormalAllocator(){}
            NormalAllocator(const NormalAllocatorConfig&){}

            auto malloc(size_t sz) -> void *
            {
                using namespace cuda_management::local_exception;

                if (sz == 0u)
                {
                    return nullptr;
                }

                void * cuda_buf = nullptr;
                cudaError_t err = cudaMalloc(static_cast<void **>(&cuda_buf), sz);

                if (err != cudaSuccess)
                {
                    throw cuda_bad_alloc();
                }

                if (cuda_buf == nullptr)
                {
                    throw cuda_corruption();
                }

                return cuda_buf;
            }

            void free(void * ptr) noexcept
            {
                using namespace cuda_management::local_exception;

                if (ptr == nullptr)
                {
                    return;
                }

                cudaFree(ptr);
            }
    };
}

#endif