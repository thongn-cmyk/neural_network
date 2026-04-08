#ifndef __CONCURRENCY_TASK_TASK_HANDLE_INTERFACE_H__
#define __CONCURRENCY_TASK_TASK_HANDLE_INTERFACE_H__

namespace concurrency_task
{
    template <class T>
    class TaskHandleInterface
    {
        public:

            virtual ~TaskHandleInterface() noexcept = default;

            virtual auto is_completed() noexcept -> bool = 0;
            virtual void interrupt() noexcept = 0;

            virtual auto wait() -> T = 0;
    };
}

#endif