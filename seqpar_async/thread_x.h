//HEADER_CONTROL 0

#ifndef __THREAD_X_H__
#define __THREAD_X_H__

#include <stdint.h>
#include <stdlib.h>
#include <mutex>
#include <exception>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <deque>
#include <semaphore>
#include <mutex_extension/fair_mutex.h>
#include <memory>
#include <iostream>
#include <concurrency_base/concurrency_base.h>
#include <common_exception/common_exception.h>
#include <concurrent_queue/bounded_queue.h>

namespace thread_x
{
    class ExecutableInterface
    {
        public:

            virtual ~ExecutableInterface() noexcept = default;
            virtual void run() noexcept = 0;
    };

    class ThreadPoolManagerInterface
    {
        public:
        
            virtual ~ThreadPoolManagerInterface() noexcept = default;
            virtual void set_thread_worker(size_t) = 0;
            virtual void run(std::unique_ptr<ExecutableInterface>&&) = 0;
    };

    class WorkOrderContainerInterface
    {
        public:

            virtual ~WorkOrderContainerInterface() noexcept = default;

            virtual void push(std::unique_ptr<ExecutableInterface>&&) = 0;
            virtual auto pop() noexcept -> std::unique_ptr<ExecutableInterface> = 0;
            virtual void poison() noexcept = 0;
    };

    class Worker: public virtual concurrency_base::WorkerInterface
    {
        private:

            std::shared_ptr<WorkOrderContainerInterface> workorder_container;

        public:

            Worker(std::shared_ptr<WorkOrderContainerInterface> workorder_container)
            {
                if (workorder_container == nullptr)
                {
                    throw std::invalid_argument("bad container, null");
                }

                this->workorder_container = std::move(workorder_container);
            }

            auto run_one_epoch() noexcept -> bool
            {
                std::unique_ptr<ExecutableInterface> workorder = this->workorder_container->pop();

                if (workorder == nullptr)
                {
                    return true;
                }

                workorder->run();

                return true;
            }
    };

    auto get_worker(std::shared_ptr<WorkOrderContainerInterface> workorder_container) -> std::shared_ptr<void>
    {
        auto worker_resource = concurrency_base::daemon_saferegister(concurrency_base::ASYNC_SEQPAR_DAEMON,
                                                                     std::make_unique<Worker>(std::move(workorder_container)));

        if (!worker_resource.has_value())
        {
            common_exception::throw_exception(worker_resource.error());
        }

        return std::make_shared<decltype(worker_resource)>(std::move(worker_resource));
    }

    class TwoWayWorkOrderContainer: public virtual WorkOrderContainerInterface
    {
        private:

            concurrent_queue::bounded_queue::BoundedQueue<std::unique_ptr<ExecutableInterface>> base;

        public:

            TwoWayWorkOrderContainer(size_t workorder_container_cap): base(workorder_container_cap){}

            void push(std::unique_ptr<ExecutableInterface>&& arg)
            {
                if (arg == nullptr)
                {
                    throw std::invalid_argument("bad argument, null");
                }

                this->base.push(std::move(arg));
            }

            auto pop() noexcept -> std::unique_ptr<ExecutableInterface>
            {
                std::optional<std::unique_ptr<ExecutableInterface>> rs = this->base.pop();

                if (!rs.has_value())
                {
                    return nullptr;
                }

                return std::move(rs.value());
            }

            void poison() noexcept
            {
                this->base.poison();
            }
    };

    class WorkOrderContainerFactory
    {
        public:

            static auto get_two_way_workorder_container(size_t container_capacity) -> std::unique_ptr<WorkOrderContainerInterface>
            {
                return std::make_unique<TwoWayWorkOrderContainer>(container_capacity);
            }
    };

    class NormalThreadPoolManager: public virtual ThreadPoolManagerInterface
    {
        private:

            std::shared_ptr<WorkOrderContainerInterface> workorder_container;
            std::vector<std::shared_ptr<void>> worker_vec;

        public:

            NormalThreadPoolManager(std::unique_ptr<WorkOrderContainerInterface> workorder_container): workorder_container(std::move(workorder_container)),
                                                                                                       worker_vec()
            {    
                if (this->workorder_container == nullptr)
                {
                    throw std::invalid_argument("bad container, null");
                }
            }

            ~NormalThreadPoolManager() noexcept
            {
                this->workorder_container->poison();
                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->worker_vec.clear();
            }

            NormalThreadPoolManager(const NormalThreadPoolManager&) = delete;
            NormalThreadPoolManager& operator =(const NormalThreadPoolManager&) = delete;

            void set_thread_worker(size_t sz)
            {
                if (sz <= this->worker_vec.size())
                {
                    this->worker_vec.resize(sz);
                    return;
                }

                size_t org_sz           = this->worker_vec.size();
                size_t new_worker_sz    = sz - org_sz;

                try
                {
                    for (size_t i = 0u; i < new_worker_sz; ++i)
                    {
                        this->worker_vec.push_back(get_worker(this->workorder_container));
                    }
                }
                catch (...)
                {
                    this->worker_vec.resize(org_sz);
                    throw;
                }
            }

            void run(std::unique_ptr<ExecutableInterface>&& arg)
            {
                this->workorder_container->push(std::move(arg));
            }
    };

    class ThreadPoolFactory
    {
        public:

            static auto get_normal_thread_pool_manager(size_t workorder_container_capacity) -> std::unique_ptr<ThreadPoolManagerInterface>
            {
                return std::make_unique<NormalThreadPoolManager>(WorkOrderContainerFactory::get_two_way_workorder_container(workorder_container_capacity));
            }
    };
}

#endif