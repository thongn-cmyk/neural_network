#ifndef __CONCURRENCY_DETACHABLE_TASK_DETACHABLE_TASK_HANDLE_INTERFACE_H__
#define __CONCURRENCY_DETACHABLE_TASK_DETACHABLE_TASK_HANDLE_INTERFACE_H__

#include <concurrency_task/task_handle_interface.h>

namespace concurrency_detachable_task
{
    template <class T>
    class DetachableTaskHandleInterface: public virtual concurrency_task::TaskHandleInterface<T>
    {
        public:

            virtual ~DetachableTaskHandleInterface() noexcept = default;

            virtual void detach() noexcept = 0;
    };
}

#endif