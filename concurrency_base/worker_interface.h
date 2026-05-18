#ifndef __CONCURRENCY_BASE_WORKER_INTERFACE_H__
#define __CONCURRENCY_BASE_WORKER_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

namespace concurrency_base::interface
{
    struct WorkerInterface
    {
        virtual ~WorkerInterface() noexcept = default;

        virtual bool run_one_epoch() noexcept = 0;
    };
}

#endif