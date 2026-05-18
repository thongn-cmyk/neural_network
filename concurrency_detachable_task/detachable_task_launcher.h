#ifndef __CONCURRENCY_DETACHABLE_TASK_DETACHABLE_LAUNCHER_H__
#define __CONCURRENCY_DETACHABLE_TASK_DETACHABLE_LAUNCHER_H__

#include "detachable_task_handle_interface.h"
#include <concurrency_task/task_launcher.h>
#include <resource_disposer/resource_disposer.h>
#include <memory>

namespace concurrency_detachable_task
{
    template <class T>
    class DetachableTaskHandle: public virtual DetachableTaskHandleInterface<T>
    {
        private:

            std::unique_ptr<concurrency_task::TaskHandleInterface<T>> base;

        public:

            DetachableTaskHandle(std::unique_ptr<concurrency_task::TaskHandleInterface<T>>&& base_arg)
            {
                if (base_arg == nullptr)
                {
                    throw std::invalid_argument("bad base, null");
                }

                this->base = std::move(base_arg);
            }

            auto is_completed() noexcept -> bool
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                return this->base->is_completed();
            }

            void interrupt() noexcept
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                this->base->interrupt();
            }

            auto wait() -> T
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                return this->base->wait();
            }

            void detach() noexcept
            {
                if (this->base == nullptr)
                {
                    return;
                }

                //I just think that interrupt should be here because the otherwise is a no-no

                this->base->interrupt();
                resource_disposer::dispose(std::make_unique<resource_disposer::DisposableWrapper<decltype(this->base)>>(std::move(this->base)));
                this->base = nullptr;
            }
    };

    class DetachableTaskLauncher
    {
        public:

            template <class T>
            auto launch(std::unique_ptr<concurrency_task::TaskInterface<T>> task) -> std::unique_ptr<DetachableTaskHandleInterface<T>>
            {
                auto rs = concurrency_task::TaskLauncher{}.launch(std::move(task));

                try
                {
                    return std::make_unique<DetachableTaskHandle<T>>(std::move(rs));
                }
                catch (...)
                {
                    std::abort();
                }
            }
    };
}

#endif