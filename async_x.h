//HEADER_CONTROL 1

#ifndef __ASYNC_X_H__
#define __ASYNC_X_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <thread>
#include <mutex>
#include <exception>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include "thread_x.h"
#include "stdx.h"

namespace async_x
{
    struct Signature{};

    using singleton_container = stdx::singleton_container<std::unique_ptr<thread_x::ThreadPoolManagerInterface>, Signature>;

    void init(size_t worker_sz, size_t container_sz)
    {
        singleton_container::get() = thread_x::ThreadPoolFactory::get_normal_thread_pool_manager(container_sz);
        singleton_container::get()->set_thread_worker(worker_sz);

        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    // static volatile int lazy_initializer = []
    // {
    //     constexpr size_t DEFAULT_WORKER_SZ      = 8u;
    //     constexpr size_t DEFAULT_CONTAINER_SZ   = 128u;

    //     init(DEFAULT_WORKER_SZ, DEFAULT_CONTAINER_SZ);

    //     return 1;
    // }();

    void deinit()
    {
        std::atomic_thread_fence(std::memory_order_seq_cst);

        singleton_container::get() = nullptr;
    }

    template <class Task>
    class SynchronousWorkOrder: public virtual thread_x::ExecutableInterface
    {
        private:

            std::shared_ptr<std::binary_semaphore> smp;
            Task task;
        
        public:

            static_assert(std::is_nothrow_invocable_v<Task>);

            SynchronousWorkOrder(std::shared_ptr<std::binary_semaphore> smp,
                                 Task task): smp(std::move(smp)),
                                             task(std::move(task)){}

            void run() noexcept
            {
                this->task();
                this->smp->release();
            }
    };

    struct SynchronizationFacility
    {
        std::shared_ptr<std::binary_semaphore> smp;
    };

    template <class Task>
    static auto make_thread(Task&& task) -> std::shared_ptr<void>
    {
        std::shared_ptr<std::binary_semaphore> smp = std::make_shared<std::binary_semaphore>(0);
        SynchronousWorkOrder workorder(smp, std::forward<Task>(task));

        auto destructor = [](void * polymorphic_obj) noexcept
        {
            SynchronizationFacility * obj = static_cast<SynchronizationFacility *>(polymorphic_obj);
            obj->smp->acquire();

            delete obj;
        };

        std::unique_ptr<thread_x::ExecutableInterface> executable = std::make_unique<decltype(workorder)>(std::move(workorder));

        try
        {
            singleton_container::get()->run(std::move(executable));
        }
        catch (...)
        {
            throw;
        }

        try
        {
            return std::unique_ptr<void, decltype(destructor)>(static_cast<void *>(new SynchronizationFacility(SynchronizationFacility{.smp = smp})),
                                                               destructor);
        }
        catch (...)
        {
            std::abort();
        }
    }

    template <class Iterator, class Resolutor>
    void sequential_parallel_launch(Iterator first, Iterator last, Resolutor&& resolutor)
    {
        std::optional<std::exception_ptr> exec_exception = std::nullopt;
        std::mutex exec_mtx{};

        {
            std::vector<std::shared_ptr<void>> work_vec{};

            for (auto it = first; it != last; ++it)
            {
                auto noexcept_resolutor = [it, &exec_exception, &exec_mtx, &resolutor]() noexcept
                {
                    try
                    {
                        resolutor(*it);
                    }
                    catch (...)
                    {
                        std::lock_guard<std::mutex> lck_grd(exec_mtx);
                        exec_exception = std::current_exception();
                    }
                };

                work_vec.push_back(make_thread(noexcept_resolutor));
            }
        }

        if (exec_exception.has_value())
        {
            std::rethrow_exception(exec_exception.value());
        }
    }

    template <class Iterator, class Resolutor>
    void sequential_parallel_group_launch(Iterator first, Iterator last, Resolutor&& resolutor,
                                          size_t max_group_count = 8u)
    {
        if (max_group_count == 0u)
        {
            throw std::invalid_argument("bad max_group_count, 0");
        }

        size_t group_count;

        if (max_group_count == 1u)
        {
            group_count = 1u;
        }
        else
        {
            group_count = max_group_count - 1u;
        }

        auto discrete_group_vec = std::vector<std::pair<Iterator, Iterator>>{};

        size_t sz               = std::distance(first, last);
        size_t group_sz         = sz / group_count;
        size_t lastest_last     = 0u;

        auto new_resolutor  = [&](const std::pair<Iterator, Iterator>& resolution_range)
        {
            for (auto it = resolution_range.first; it != resolution_range.second; ++it)
            {
                resolutor(*it);
            }
        };

        for (size_t i = 0u; i < group_count; ++i)
        {
            size_t idx_first    = group_sz * i;
            size_t idx_last     = group_sz * (i + 1);
            lastest_last        = idx_last;

            if (idx_first != idx_last)
            {
                discrete_group_vec.push_back({std::next(first, idx_first), std::next(first, idx_last)});
            }
        }

        if (lastest_last != sz)
        {
            discrete_group_vec.push_back({std::next(first, lastest_last), last});
        }

        sequential_parallel_launch(discrete_group_vec.begin(), discrete_group_vec.end(), new_resolutor);
    }

    //I think the problem that we have is with the un-even launches
    //so for the launch_2, we'd just individualize the launches

    template <class Iterator, class Resolutor>
    void sequential_parallel_group_launch_2(Iterator first, Iterator last, Resolutor&& resolutor,
                                            size_t tentative_group_count = 8u)
    {
        const size_t MIN_GROUP_COUNT    = size_t{1};
        const size_t MAX_GROUP_COUNT    = size_t{1} << 10;

        size_t group_count              = std::clamp(tentative_group_count, MIN_GROUP_COUNT, MAX_GROUP_COUNT);
        auto discrete_group_vec         = std::vector<std::pair<Iterator, Iterator>>{};

        size_t sz                       = std::distance(first, last);
        size_t group_sz                 = sz / group_count;
        size_t lastest_last             = 0u;

        auto new_resolutor = [&](const std::pair<Iterator, Iterator>& resolution_range)
        {
            for (auto it = resolution_range.first; it != resolution_range.second; ++it)
            {
                resolutor(*it);
            }
        };

        for (size_t i = 0u; i < group_count; ++i)
        {
            size_t idx_first    = group_sz * i;
            size_t idx_last     = group_sz * (i + 1);
            lastest_last        = idx_last;

            if (idx_first != idx_last)
            {
                discrete_group_vec.push_back({std::next(first, idx_first), std::next(first, idx_last)});
            }
        }

        for (size_t i = lastest_last; i < sz; ++i)
        {
            discrete_group_vec.push_back({std::next(first, i), std::next(first, i + 1)});
        }

        sequential_parallel_launch(discrete_group_vec.begin(), discrete_group_vec.end(), new_resolutor);
    }
}

#endif