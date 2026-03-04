//HEADER_CONTROL 1

#ifndef __ASYNC_CUDA_X_H__
#define __ASYNC_CUDA_X_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>
#include <functional>
#include <utility>
#include <algorithm>
#include <mutex>
#include <semaphore>
#include <deque>
#include "fair_mutex.h"
#include "stdx.h"

namespace async_cuda_x
{
    struct cuda_unavailable_exception: std::exception{};
    struct cuda_runtime_exception: std::exception{};

    using cuda_async_exception_t = uint8_t;

    static inline constexpr cuda_async_exception_t SUCCESS                  = 0b00000000;
    static inline constexpr cuda_async_exception_t CUDA_NOT_AVAILABLE       = 0b00000001;
    static inline constexpr cuda_async_exception_t CUDA_DEVICE_CORRUPTED    = 0b00000010;
    static inline const std::chrono::nanoseconds PANIC_GRACE_PERIOD         = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1));

    auto sync_cuda_device() noexcept -> cuda_async_exception_t
    {
        return SUCCESS;
    }

    void throw_err(cuda_async_exception_t err)
    {
        switch (err)
        {
            case SUCCESS:
            {
                break;
            }
            case CUDA_NOT_AVAILABLE:
            {
                throw cuda_unavailable_exception{};
            }
            case CUDA_DEVICE_CORRUPTED:
            {
                throw cuda_runtime_exception{};
            }
            default:
            {
                throw std::runtime_error("unknown error code");
            }
        }
    }

    class CudaExecutableInterface
    {
        public:

            virtual ~CudaExecutableInterface() = default;
            virtual void run() noexcept = 0;
    };

    class NotifiableInterface
    {
        public:

            virtual ~NotifiableInterface() = default;
            virtual void notify(cuda_async_exception_t) noexcept = 0;
    };

    class InternalCudaExecutableInterface: public virtual CudaExecutableInterface,
                                           public virtual NotifiableInterface{};

    class CudaExecutableContainerInterface
    {
        public:

            virtual ~CudaExecutableContainerInterface() = default;
            virtual void push(std::unique_ptr<InternalCudaExecutableInterface>&&) = 0;
            virtual auto pop(size_t sz, std::chrono::nanoseconds wait_time) -> std::vector<std::unique_ptr<InternalCudaExecutableInterface>> = 0;
            virtual void poison() noexcept = 0;
    };

    class CudaAsyncLauncherInterface
    {
        public:

            virtual ~CudaAsyncLauncherInterface() = default;
            virtual void launch(std::unique_ptr<CudaExecutableInterface>&&) = 0;
    };

    class CudaSingleSubscriberExecutableContainer: public virtual CudaExecutableContainerInterface
    {
        private:

            struct ObserverDemand
            {
                std::binary_semaphore * release_smp;
                std::vector<std::unique_ptr<InternalCudaExecutableInterface>> * dst;
                size_t wakeup_sz;
            };

            std::deque<std::unique_ptr<InternalCudaExecutableInterface>> work_order_container;
            std::optional<ObserverDemand> observer_demand;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            bool is_poisoned;

        public:

            CudaSingleSubscriberExecutableContainer(): work_order_container(),
                                                       observer_demand(),
                                                       mtx(fair_mutex::make_unique_fair_atomic_flag()),
                                                       is_poisoned(false){}

            void push(std::unique_ptr<InternalCudaExecutableInterface>&& item)
            {
                if (item == nullptr)
                {
                    throw std::invalid_argument("bad item, null item");
                }

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->is_poisoned)
                    {
                        throw std::runtime_error("container poisoned");
                    }

                    this->work_order_container.push_back(std::move(item));

                    if (this->observer_demand.has_value())
                    {
                        if (this->work_order_container.size() >= this->observer_demand->wakeup_sz)
                        {
                            try
                            {
                                std::copy(std::make_move_iterator(this->work_order_container.begin()), 
                                          std::make_move_iterator(std::next(this->work_order_container.begin(), this->observer_demand->wakeup_sz)),
                                          std::back_inserter(*this->observer_demand->dst));
                            }
                            catch (...)
                            {
                                std::abort();
                            }

                            this->work_order_container.erase(this->work_order_container.begin(),
                                                             std::next(this->work_order_container.begin(), this->observer_demand->wakeup_sz));

                            this->observer_demand->release_smp->release();
                            this->observer_demand = std::nullopt;
                        }
                    }
                }
            }

            auto pop(size_t sz, std::chrono::nanoseconds wait_time) -> std::vector<std::unique_ptr<InternalCudaExecutableInterface>>
            {
                std::vector<std::unique_ptr<InternalCudaExecutableInterface>> container{};
                std::binary_semaphore smp(0);

                bool need_acquire = [&]()
                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->is_poisoned)
                    {
                        throw std::runtime_error("container poisoned");
                    }

                    if (this->work_order_container.size() >= sz)
                    {
                        try
                        {
                            std::copy(std::make_move_iterator(this->work_order_container.begin()),
                                      std::make_move_iterator(std::next(this->work_order_container.begin(), sz)),
                                      std::back_inserter(container));
                        }
                        catch (...)
                        {
                            std::abort();
                        }

                        this->work_order_container.erase(this->work_order_container.begin(),
                                                         std::next(this->work_order_container.begin(), sz));

                        return false;
                    }
                    else
                    {
                        if (this->observer_demand.has_value())
                        {
                            std::abort();
                        }
                        else
                        {
                            this->observer_demand = ObserverDemand
                            {
                                .release_smp    = &smp,
                                .dst            = &container,
                                .wakeup_sz      = sz
                            };

                            return true;
                        }
                    }
                }();

                if (need_acquire)
                {
                    bool did_acquire = smp.try_acquire_for(wait_time);

                    if (!did_acquire)
                    {
                        fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                        size_t container_desired_sz = sz - container.size();
                        size_t max_peek_sz          = std::min(this->work_order_container.size(), container_desired_sz);

                        try
                        {
                            std::copy(std::make_move_iterator(this->work_order_container.begin()),
                                      std::make_move_iterator(std::next(this->work_order_container.begin(), max_peek_sz)),
                                      std::back_inserter(container));
                        }
                        catch (...)
                        {
                            std::abort();
                        }

                        this->work_order_container.erase(this->work_order_container.begin(),
                                                         std::next(this->work_order_container.begin(), max_peek_sz));

                        this->observer_demand = std::nullopt;
                    }
                }

                return container;
            }

            void poison() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->observer_demand.has_value())
                {
                    *this->observer_demand->dst = std::vector<std::unique_ptr<InternalCudaExecutableInterface>>{};
                    this->observer_demand->release_smp->release();
                    this->observer_demand       = std::nullopt;
                }

                this->is_poisoned = true;
            }
    };

    class ExecutableContainerFactory
    {
        public:

            static auto get_single_subscriber_executable_container() -> std::unique_ptr<CudaExecutableContainerInterface>
            {
                return std::make_unique<CudaSingleSubscriberExecutableContainer>();
            }
    };

    class CudaWorker
    {
        private:

            std::shared_ptr<CudaExecutableContainerInterface> workorder_container;
            size_t consumption_sz;
            std::chrono::nanoseconds consumption_wait_time;
            std::atomic<bool> poison_pill;

        public:

            CudaWorker(std::shared_ptr<CudaExecutableContainerInterface> workorder_container,
                       size_t consumption_sz,
                       std::chrono::nanoseconds consumption_wait_time): workorder_container(std::move(workorder_container)),
                                                                        consumption_sz(consumption_sz),
                                                                        consumption_wait_time(consumption_wait_time),
                                                                        poison_pill(false){}

            void run() noexcept
            {
                while (true)
                {
                    bool poison_status = this->poison_pill.load(std::memory_order_relaxed);

                    if (poison_status)
                    {
                        break;
                    }

                    try
                    {
                        std::vector<std::unique_ptr<InternalCudaExecutableInterface>> executable_vec = this->workorder_container->pop(this->consumption_sz, this->consumption_wait_time);

                        for (const auto& executable: executable_vec)
                        {
                            executable->run();
                        }

                        cuda_async_exception_t sync_err = sync_cuda_device();

                        for (const auto& executable: executable_vec)
                        {
                            executable->notify(sync_err);
                        }
                    }   
                    catch (...)
                    {
                        std::this_thread::sleep_for(PANIC_GRACE_PERIOD);
                        continue;
                    }                 
                }

                std::atomic_thread_fence(std::memory_order_seq_cst);
            }

            void stop() noexcept
            {
                this->poison_pill.exchange(true, std::memory_order_relaxed);
            }
    };

    struct CudaWorkerHandle
    {
        std::shared_ptr<CudaWorker> worker;
        std::shared_ptr<std::thread> thr;
    };

    static auto make_cuda_worker(std::shared_ptr<CudaExecutableContainerInterface> workorder_container,
                                 size_t consumption_sz,
                                 std::chrono::nanoseconds consumption_wait_time) -> std::shared_ptr<void>
    {
        const size_t MIN_CONSUMPTION_SZ                             = 1u;
        const size_t MAX_CONSUMPTION_SZ                             = size_t{1} << 30;
        const std::chrono::nanoseconds MIN_CONSUMPTION_WAIT_TIME    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::nanoseconds(0));
        const std::chrono::nanoseconds MAX_CONSUMPTION_WAIT_TIME    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1));

        if (workorder_container == nullptr)
        {
            throw std::invalid_argument("bad workorder container, null container");
        }

        if (std::clamp(consumption_sz, MIN_CONSUMPTION_SZ, MAX_CONSUMPTION_SZ) != consumption_sz)
        {
            throw std::invalid_argument("bad consumption size, out of range size");
        }

        if (std::clamp(consumption_wait_time, MIN_CONSUMPTION_WAIT_TIME, MAX_CONSUMPTION_WAIT_TIME) != consumption_wait_time)
        {
            throw std::invalid_argument("bad consumption wait time, out of range wait time");
        }

        auto destructor = [](void * arg) noexcept
        {
            CudaWorkerHandle * worker_handle = static_cast<CudaWorkerHandle *>(arg);
            worker_handle->worker->stop();
            std::atomic_signal_fence(std::memory_order_seq_cst);
            worker_handle->thr->join();

            delete worker_handle;
        };

        std::shared_ptr<CudaWorker> cuda_worker = std::make_shared<CudaWorker>(workorder_container, consumption_sz, consumption_wait_time);
        auto cuda_runner = [=]() noexcept
        {
            cuda_worker->run();
        };
        std::shared_ptr<std::thread> thr        = std::make_shared<std::thread>(cuda_runner);
        CudaWorkerHandle * cuda_worker_handle; 

        try
        {
            cuda_worker_handle = new CudaWorkerHandle
            (
                CudaWorkerHandle
                {
                    .worker = cuda_worker,
                    .thr    = thr
                }
            );
        }
        catch (...)
        {
            cuda_worker->stop();
            std::atomic_signal_fence(std::memory_order_seq_cst);
            thr->join();
            throw;
        }

        return std::unique_ptr<void, decltype(destructor)>
        (
            static_cast<void *>(cuda_worker_handle),
            destructor
        );
    }

    class CudaAsyncLauncher: public virtual CudaAsyncLauncherInterface
    {
        private:

            std::shared_ptr<CudaExecutableContainerInterface> workorder_container;
            std::shared_ptr<void> cuda_worker;

        public:

            CudaAsyncLauncher(std::shared_ptr<CudaExecutableContainerInterface> workorder_container,
                              std::shared_ptr<void> cuda_worker): workorder_container(std::move(workorder_container)),
                                                                  cuda_worker(std::move(cuda_worker)){}

            ~CudaAsyncLauncher() noexcept
            {
                this->workorder_container->poison();
                this->cuda_worker = nullptr;
            }

            void launch(std::unique_ptr<CudaExecutableInterface>&& arg)
            {
                if (arg == nullptr)
                {
                    throw std::invalid_argument("bad cuda launch, null argument");
                }

                std::binary_semaphore smp(0);
                cuda_async_exception_t err;
                std::unique_ptr<InternalCudaExecutableInterface> wrapped_arg = std::make_unique<InternalCudaExecutable>(std::move(arg), &smp, &err);

                this->workorder_container->push(std::move(wrapped_arg));

                smp.acquire();

                if (err != SUCCESS)
                {
                    throw_err(err);
                }
            }

        private:
            
            class InternalCudaExecutable: public virtual InternalCudaExecutableInterface
            {
                private:

                    std::unique_ptr<CudaExecutableInterface> base;
                    std::binary_semaphore * smp;
                    cuda_async_exception_t * err;

                public:

                    InternalCudaExecutable(std::unique_ptr<CudaExecutableInterface>&& base,
                                           std::binary_semaphore * smp,
                                           cuda_async_exception_t * err): base(std::move(base)),
                                                                          smp(smp),
                                                                          err(err){}

                    void run() noexcept
                    {
                        this->base->run();
                    }

                    void notify(cuda_async_exception_t err) noexcept
                    {
                        *this->err = err;
                        this->smp->release();
                    }
            };
    };

    class LauncherFactory
    {
        public:

            static auto get_cuda_async_launcher(size_t cuda_sync_frequency = 1000u,
                                                size_t sync_batch_sz = 1024u) -> std::unique_ptr<CudaAsyncLauncherInterface>
            {
                const size_t MIN_CUDA_FREQUENCY     = 1u;
                const size_t MAX_CUDA_FREQUENCY     = size_t{1} << 30;
                const size_t MIN_SYNC_BATCH_SZ      = 1u;
                const size_t MAX_SYNC_BATCH_SZ      = size_t{1} << 30;

                if (std::clamp(cuda_sync_frequency, MIN_CUDA_FREQUENCY, MAX_CUDA_FREQUENCY) != cuda_sync_frequency)
                {
                    throw std::invalid_argument("bad cuda sync frequency, frequency out of range");
                }

                if (std::clamp(sync_batch_sz, MIN_SYNC_BATCH_SZ, MAX_SYNC_BATCH_SZ) != sync_batch_sz)
                {
                    throw std::invalid_argument("bad sync batch size, size out of range");
                }

                std::chrono::nanoseconds tentative_wave_length              = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)) / cuda_sync_frequency;
                std::shared_ptr<CudaExecutableContainerInterface> container = ExecutableContainerFactory::get_single_subscriber_executable_container();
                std::chrono::nanoseconds wave_length                        = std::max(tentative_wave_length, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1)));

                return std::make_unique<CudaAsyncLauncher>(container,
                                                           make_cuda_worker(container, sync_batch_sz, wave_length));
            }
    };

    struct Signature{};
    using singleton_object = stdx::singleton_container<std::unique_ptr<CudaAsyncLauncherInterface>, Signature>;

    void init()
    {
        singleton_object::get() = LauncherFactory::get_cuda_async_launcher();
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    // static volatile int lazy_initializer = []
    // {
    //     singleton_object::get() = LauncherFactory::get_cuda_async_launcher();
    //     return 1;
    // }();

    void deinit() noexcept
    {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        singleton_object::get() = nullptr;
    }

    void launch(std::unique_ptr<CudaExecutableInterface>&& arg)
    {
        singleton_object::get()->launch(std::move(arg));
    }
}

#endif