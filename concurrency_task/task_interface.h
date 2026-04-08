#ifndef __CONCURRENCY_TASK_TASK_INTERFACE_H__
#define __CONCURRENCY_TASK_TASK_INTERFACE_H__

#include <common_exception/cancellation_token.h>

namespace concurrency_task
{
    template <class T>
    class TaskInterface
    {
        public:

            virtual ~TaskInterface() noexcept = default;
            virtual auto run(common_exception::CancellationTokenInterface& cancellation_token) -> T = 0;
    };
}

#endif