#ifndef __CLIENT_BOX_TASK_BOX_TASK_BOX_H__
#define __CLIENT_BOX_TASK_BOX_TASK_BOX_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <optional>
#include <exception>
#include <concurrency_detachable_task/detachable_task_launcher.h>
#include "local_exception.h"

namespace client_box::task_box
{
    template <class T>
    using Task      = concurrency_task::TaskInterface<T>;

    template <class T>
    using Promise   = concurrency_detachable_task::DetachableTaskHandleInterface<T>;

    template <class WorkOrderType, class ResultType>
    class ClientTaskBox
    {
        protected:

            virtual auto make_task(const WorkOrderType& run_work_order) -> Task<ResultType> = 0;

        private:

            std::unique_ptr<Promise<ResultType>> promise;
            bool was_explicitly_destroyed;
        
        public:

            ClientTaskBox(): promise(),
                             was_explicitly_destroyed(false){}

            ~ClientTaskBox() noexcept
            {
                this->close(false);
            }

            void run(const WorkOrderType& run_work_order)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                if (this->promise != nullptr)
                {
                    throw second_run_error{};
                }

                using TaskLauncher  = concurrency_detachable_task::DetachableTaskLauncher;

                this->promise       = TaskLauncher{}.launch(this->make_task(run_work_order));
            }

            auto is_completed() -> bool
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                if (this->promise == nullptr)
                {
                    throw run_not_invoked_error{};
                }

                return this->promise->is_completed();
            }

            void interrupt() noexcept
            {
                if (this->was_explicitly_destroyed)
                {
                    return;
                }

                if (this->promise == nullptr)
                {
                    return;
                }

                this->promise->interrupt();
            }

            auto wait() -> ResultType
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                if (this->promise == nullptr)
                {
                    throw run_not_invoked_error{};
                }

                return this->promise->wait();
            }

            void close(bool hard_close = true) noexcept
            {
                if (std::exchange(this->was_explicitly_destroyed, true))
                {
                    return;
                }

                if (this->promise == nullptr)
                {
                    return;
                }

                if (hard_close)
                {
                    this->promise->interrupt();
                    this->promise = nullptr;
                }
                else
                {
                    this->promise->detach();
                }
            }
    };
}

#endif