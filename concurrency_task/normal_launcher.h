#ifndef __CONCURRENCY_TASK_TASK_LAUNCHER_H__
#define __CONCURRENCY_TASK_TASK_LAUNCHER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "task_handle_interface.h"
#include "task_interface.h"
#include <concurrency_base/concurrency_base.h>
#include <common_exception/cancellation_token.h>
#include <optional>
#include <atomic>
#include <exception>
#include <stdexcept>

namespace concurrency_task::normal_launcher
{
    template <class T>
    class InterruptableTaskWrapper: public virtual concurrency_task::TaskInterface<T>
    {
        private:

            std::shared_ptr<common_exception::CancellationTokenInterface> task_cancellation_token;
            std::shared_ptr<concurrency_task::TaskInterface<T>> base_task;

        public:

            InterruptableTaskWrapper(const std::shared_ptr<common_exception::CancellationTokenInterface>& task_cancellation_token_arg,
                                     const std::shared_ptr<concurrency_task::TaskInterface<T>>& base_task_arg)
            {
                if (task_cancellation_token_arg == nullptr)
                {
                    throw std::invalid_argument("bad task cancellation token, null");
                }

                if (base_task_arg == nullptr)
                {
                    throw std::invalid_argument("bad base task, null");
                }

                this->task_cancellation_token   = task_cancellation_token_arg;
                this->base_task                 = base_task_arg;
            }

            auto run(common_exception::CancellationTokenInterface& cancellation_token) -> T
            {
                InternalCancellationToken full_cancellation_token(this->task_cancellation_token.get(), &cancellation_token);
                return this->base_task->run(full_cancellation_token);
            }

        private:

            class InternalCancellationToken: public virtual common_exception::CancellationTokenInterface
            {
                private:

                    common_exception::CancellationTokenInterface * cancellation_token_1;
                    common_exception::CancellationTokenInterface * cancellation_token_2;

                public:

                    InternalCancellationToken(common_exception::CancellationTokenInterface * cancellation_token_1,
                                              common_exception::CancellationTokenInterface * cancellation_token_2): cancellation_token_1(stdx::safe_ptr_access(cancellation_token_1)),
                                                                                                                    cancellation_token_2(stdx::safe_ptr_access(cancellation_token_2)){}

                    auto is_canceled() noexcept -> bool
                    {
                        return this->cancellation_token_1->is_canceled() || this->cancellation_token_2->is_canceled();
                    }
            };
    };

    template <class T>
    struct Deliverable
    {
        std::optional<T> result;
        std::exception_ptr ex_ptr;
        std::atomic<bool> is_completed;
    };

    template <class T>
    class TaskAsDaemon: public virtual concurrency_base::InterruptableWorkerInterface
    {
        private:

            std::shared_ptr<Deliverable<T>> deliverable;
            std::shared_ptr<concurrency_task::TaskInterface<T>> base_task;

        public:

            TaskAsDaemon(const std::shared_ptr<Deliverable<T>>& deliverable,
                         const std::shared_ptr<concurrency_task::TaskInterface<T>>& base_task)
            {
                if (deliverable == nullptr)
                {
                    throw std::invalid_argument("bad deliverable, null");
                }                

                if (base_task == nullptr)
                {
                    throw std::invalid_argument("bad base task, null");
                }

                this->deliverable   = deliverable;
                this->base_task     = base_task;
            }

            auto run_one_epoch(common_exception::CancellationTokenInterface& cancellation_token) -> bool
            {
                try
                {
                    T result                    = this->base_task->run(cancellation_token);
                    this->deliverable->result   = std::move(result);
                    this->deliverable->ex_ptr   = nullptr;
                }
                catch (...)
                {
                    this->deliverable->ex_ptr   = std::current_exception();
                }

                this->deliverable->is_completed.exchange(true, std::memory_order_release);
                common_exception::throw_valid_exception(common_exception::OPERATION_GRACEFUL_TERMINATION_ERROR);
            }
    };

    template <class T>
    class TaskHandle: public virtual TaskHandleInterface<T>
    {
        private:

            std::shared_ptr<common_exception::CancellationToken> interruption_pill;
            std::shared_ptr<void> daemon_task;
            std::shared_ptr<Deliverable<T>> deliverable;

        public:

            TaskHandle(const std::shared_ptr<concurrency_task::TaskInterface<T>>& base_task)
            {
                if (base_task == nullptr)
                {
                    throw std::invalid_argument("bad base task, null");
                }

                this->interruption_pill         = std::make_shared<common_exception::CancellationToken>();

                this->deliverable               = std::make_shared<Deliverable<T>>();
                this->deliverable->result       = std::nullopt;
                this->deliverable->ex_ptr       = nullptr;
                this->deliverable->is_completed = false;

                auto tmp                        = concurrency_base::daemon_saferegister(concurrency_base::COMMON_ONFLY_POOL,
                                                                                        std::make_unique<TaskAsDaemon<T>>(this->deliverable,
                                                                                                                          std::make_shared<InterruptableTaskWrapper<T>>(this->interruption_pill,
                                                                                                                                                                        base_task)));

                if (!tmp.has_value())
                {
                    common_exception::throw_exception(tmp.error());
                }

                this->daemon_task       = std::move(tmp.value());
            }

            auto is_completed() noexcept -> bool
            {
                return this->deliverable->is_completed.load(std::memory_order_relaxed);
            }

            void interrupt() noexcept
            {
                this->interruption_pill->cancel();
            }

            auto wait() -> T
            {
                if (this->daemon_task == nullptr)
                {
                    throw std::invalid_argument("bad wait, second wait");
                }

                this->deliverable->is_completed.wait(false, std::memory_order_acquire);
                this->daemon_task = nullptr;
                std::atomic_signal_fence(std::memory_order_seq_cst);

                if (!this->deliverable->result.has_value())
                {
                    if (this->deliverable->ex_ptr == nullptr)
                    {
                        std::abort();
                    }

                    std::rethrow_exception(this->deliverable->ex_ptr);
                }

                return std::move(this->deliverable->result.value());
            }
    };

    class TaskLauncher
    {
        public:

            template <class T>
            auto launch(const std::shared_ptr<concurrency_task::TaskInterface<T>>& task) -> std::unique_ptr<concurrency_task::TaskHandleInterface<T>>
            {
                return std::make_unique<TaskHandle<T>>(task);
            }

    };
}

#endif