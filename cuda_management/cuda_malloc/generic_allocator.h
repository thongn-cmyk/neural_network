#ifndef __CUDA_MANAGEMENT_CUDA_MALLOC_GENERIC_ALLOCATOR_H__
#define __CUDA_MANAGEMENT_CUDA_MALLOC_GENERIC_ALLOCATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include "allocator_interface.h"
#include "dedicated_allocator.h"
#include "normal_allocator.h"
#include <variant>
#include <stl_extension/stdx.h>

namespace cuda_management::cuda_malloc
{
    struct GenericAllocatorConfig
    {
        std::variant<stdx::reflectible_monostate,
                     NormalAllocatorConfig,
                     DedicatedAllocatorConfig> config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config);
        }
    };

    class GenericAllocator: public virtual AllocatorInterface
    {
        private:

            std::unique_ptr<AllocatorInterface> base;

        public:

            GenericAllocator(const GenericAllocatorConfig& config)
            {
                if (std::holds_alternative<NormalAllocatorConfig>(config.config))
                {
                    this->base  = std::make_unique<NormalAllocator>(std::get<NormalAllocatorConfig>(config.config));
                }
                else if (std::holds_alternative<DedicatedAllocatorConfig>(config.config))
                {
                    this->base  = std::make_unique<DedicatedAllocator>(std::get<DedicatedAllocatorConfig>(config.config));
                }
                else
                {
                    throw std::invalid_argument("bad allocator config, dispatch code not found");
                }
            }

            auto malloc(size_t sz) -> void *
            {
                return this->base->malloc(sz);
            }

            void free(void * ptr) noexcept
            {
                this->base->free(ptr);
            }
    };
}

#endif