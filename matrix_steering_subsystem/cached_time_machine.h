//HEADER_CONTROL 2


#ifndef __CACHED_TIME_MACHINE_H__
#define __CACHED_TIME_MACHINE_H__

#include <stdint.h>
#include <stdlib.h>
#include "time_machine_interface.h"
#include <general_definition/float_def.h>
#include <memory>
#include <unordered_map>

namespace time_machine
{
    class CachedTimeMachine: public virtual TimeMachineInterface
    {
        private:

            std::shared_ptr<TimeMachineInterface> base;
            std::unordered_map<std_float_t, tm_float_t> cached_result;
            size_t cache_map_capacity;

        public:

            CachedTimeMachine(std::shared_ptr<TimeMachineInterface> base,
                              size_t cache_map_capacity)
            {
                stdx::safe_ptr_access(base.get());

                this->base                  = std::move(base);
                this->cached_result         = std::unordered_map<std_float_t, tm_float_t>{};
                this->cache_map_capacity    = cache_map_capacity;
            }

            auto f(std_float_t t) -> tm_float_t
            {
                if (auto map_ptr = this->cached_result.find(t); map_ptr != this->cached_result.end())
                {
                    return map_ptr->second;
                }

                tm_float_t new_result = this->base->f(t);

                if (this->cache_map_capacity != 0u)
                {
                    if (this->cached_result.size() == this->cache_map_capacity)
                    {
                        this->cached_result.clear();
                    }

                    this->cached_result.insert({t, new_result});
                }

                return new_result;
            }

            void clear_cache_map() noexcept
            {
                this->cached_result.clear();
            }
    };
}

#endif