#ifndef __CONCURRENCY_BASE_INTERRUPTABLE_WORKER_INTERFACE_H__
#define __CONCURRENCY_BASE_INTERRUPTABLE_WORKER_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include "worker_interface"

namespace concurrency_base::interface
{
    struct InterruptableWorkerInterface
    {
        virtual ~InterruptableWorkerInterface() noexcept = default;

        virtual void run_one_epoch(common_exception::CancellationTokenInterface& cancellation_token) = 0;
    };
}

#endif