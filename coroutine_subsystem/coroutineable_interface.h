#ifndef __COROUTINE_SUBSYSTEM_COROUTINEABLE_INTERFACE_H__
#define __COROUTINE_SUBSYSTEM_COROUTINEABLE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

namespace coroutine_x
{
    class CoroutineableInterface
    {
        public:

            virtual ~CoroutineableInterface() noexcept = default;

            virtual auto has_next() noexcept -> bool = 0;
            virtual auto next() noexcept -> bool = 0;
    };
}

#endif