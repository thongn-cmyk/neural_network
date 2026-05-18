#ifndef __CONCURRENCY_BASE_INTERRUPTABLE_WORKER_INTERFACE_H__
#define __CONCURRENCY_BASE_INTERRUPTABLE_WORKER_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <common_exception/cancellation_token.h>

namespace concurrency_base::interface
{
    struct InterruptableWorkerInterface
    {
        virtual ~InterruptableWorkerInterface() noexcept = default;

        virtual auto run_one_epoch(common_exception::CancellationTokenInterface& cancellation_token) -> bool = 0;
    };
}

#endif