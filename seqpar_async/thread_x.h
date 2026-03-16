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

            struct WaitingArgument
            {
                std::unique_ptr<ExecutableInterface> * dst;
                std::binary_semaphore * smp;
            };

            struct PushingArgument
            {
                std::unique_ptr<ExecutableInterface> item;
                std::binary_semaphore * smp;
            };

            std::deque<std::unique_ptr<ExecutableInterface>> workorder_container;
            size_t workorder_container_cap;
            std::deque<WaitingArgument> waiting_arg_vec;
            std::deque<PushingArgument> pushing_arg_vec;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            bool is_poisoned;

        public:

            TwoWayWorkOrderContainer(size_t workorder_container_cap): workorder_container(),
                                                                      workorder_container_cap(workorder_container_cap),
                                                                      waiting_arg_vec(),
                                                                      pushing_arg_vec(),
                                                                      mtx(fair_mutex::make_unique_fair_atomic_flag()),
                                                                      is_poisoned(false){}

            void push(std::unique_ptr<ExecutableInterface>&& arg)
            {
                if (arg == nullptr)
                {
                    throw std::invalid_argument("bad argument, null");
                }

                std::binary_semaphore waiting_smp(0);

                bool is_wait_required = [&]{
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->is_poisoned)
                    {
                        throw std::runtime_error("container poisoned");
                    }

                    if (!this->waiting_arg_vec.empty())
                    {
                        auto arg2   = std::move(this->waiting_arg_vec.front());
                        this->waiting_arg_vec.pop_front();
                        *arg2.dst    = std::move(arg);
                        arg2.smp->release();

                        return false;
                    }

                    if (this->workorder_container.size() == this->workorder_container_cap)
                    {
                        auto push_arg = PushingArgument
                        {
                            .item   = std::move(arg),
                            .smp    = &waiting_smp
                        };

                        try
                        {
                            this->pushing_arg_vec.push_back(std::move(push_arg));
                        }
                        catch (...)
                        {
                            arg = std::move(push_arg.item);
                            throw;
                        }

                        return true;
                    }

                    this->workorder_container.push_back(std::move(arg));
                    return false;
                }();

                if (is_wait_required)
                {
                    waiting_smp.acquire();
                }
            }

            auto pop() noexcept -> std::unique_ptr<ExecutableInterface>
            {
                std::unique_ptr<ExecutableInterface> wait_item;
                std::binary_semaphore waiting_smp(0);

                bool is_wait_required = [&]
                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->is_poisoned)
                    {
                        wait_item = nullptr;

                        return false;
                    }

                    if (!this->pushing_arg_vec.empty())
                    {
                        auto arg    = std::move(this->pushing_arg_vec.front());
                        this->pushing_arg_vec.pop_front();
                        arg.smp->release();
                        wait_item   = std::move(arg.item);

                        return false;
                    }

                    if (!this->workorder_container.empty())
                    {
                        auto arg    = std::move(this->workorder_container.front());
                        this->workorder_container.pop_front();
                        wait_item   = std::move(arg);

                        return false;
                    }

                    auto waiting_arg = WaitingArgument
                    {
                        .dst    = &wait_item,
                        .smp    = &waiting_smp
                    };

                    this->waiting_arg_vec.push_back(std::move(waiting_arg));

                    return true;
                }();

                if (is_wait_required)
                {
                    waiting_smp.acquire();
                }

                return wait_item;
            }

            void poison() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                for (const auto& waiting_arg: this->waiting_arg_vec)
                {
                    *waiting_arg.dst = nullptr;
                    waiting_arg.smp->release();
                }

                for (const auto& pushing_arg: this->pushing_arg_vec)
                {
                    pushing_arg.smp->release();
                }

                this->is_poisoned = true;
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