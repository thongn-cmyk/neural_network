#ifndef __COROUTINE_SUBSYSTEM_COROUTINE_WAITABLE_INTERFACE_H__
#define __COROUTINE_SUBSYSTEM_COROUTINE_WAITABLE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

namespace coroutine_x
{
    class CoroutineWaitableInterface
    {
        public:

            virtual ~CoroutineWaitableInterface() noexcept = default;

            virtual auto is_completed() noexcept -> bool = 0;
            virtual void wait() noexcept = 0;
    };
}

#endif