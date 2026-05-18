#ifndef __CONCURRENCY_BASE_RESCHEDULER_INTERFACE_H__
#define __CONCURRENCY_BASE_RESCHEDULER_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

namespace concurrency_base::interface
{
    struct ReschedulerInterface
    {
        virtual ~ReschedulerInterface() noexcept = default;

        virtual void reschedule() noexcept = 0;
    };
}

#endif