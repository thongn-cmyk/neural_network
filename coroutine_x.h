#ifndef __COROUTINE_X_H__
#define __COROUTINE_X_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include "stdx.h"
#include <chrono>
#include "fair_mutex.h"
#include <optional>
#include <deque>
#include <semaphore>
#include <thread>

namespace coroutine_x
{
    //coroutine is a process of serially processing C# async functions, where each function must call recursively into the coroutine_x and get an instance of delay_waitable
    //every function must be detached, or exitable blocked, depending on the topic in order for this to work seamlessly
    //we'd add "consume" functionality of coroutine_x later when we see fit
    //for now just make it simple stupid

    //the memory ordering is very simple, if you make it shared_ptr + multithread == auto mutex for every function access
    //if you transfer the memory per unique_ptr<> and requires the process to sync the non-threadsafes, then a release for the unique followed by an acquire at the disposal time is required
    //the acquisition of the unique_ptr must take a form of the acquire + release sequence in order for the acquirer to correctly hold the reference that unique_ptr<> points to

    //always uses shared_ptr if the object is multi-thread bound, because shared_ptr<> will acquire your resource on destruction, which is a requirement

    static inline const std::chrono::nanoseconds COROUTINE_DELAY = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(100));

    class CoroutineableInterface
    {
        public:

            virtual ~CoroutineableInterface() noexcept = default;

            virtual auto has_next() noexcept -> bool = 0;
            virtual auto next() noexcept -> bool = 0;
    };

    class UpdatableInterface
    {
        public:

            virtual ~UpdatableInterface() noexcept = default;

            virtual void update() noexcept = 0;
    };

    class CoroutineableManagerInterface: public virtual UpdatableInterface
    {
        public:

            virtual ~CoroutineableManagerInterface() noexcept = default;

            virtual void add(std::shared_ptr<CoroutineableInterface> coroutineable,
                             std::optional<std::chrono::time_point<std::chrono::system_clock>> wakeup_time) noexcept = 0;

            virtual auto get() noexcept -> std::shared_ptr<CoroutineableInterface> = 0;
            virtual void poison() noexcept = 0;
    };

    class LauncherInterface
    {
        public:

            virtual ~LauncherInterface() noexcept = default;
            virtual void add(std::shared_ptr<CoroutineableInterface> coroutineable) noexcept = 0;
    };

    class CoroutineableManager: public virtual CoroutineableManagerInterface
    {
        private:

            struct PriorityBucket
            {
                std::shared_ptr<CoroutineableInterface> item;
                std::chrono::time_point<std::chrono::system_clock> wakeup_time;
            };

            struct WaitBucket
            {
                std::shared_ptr<CoroutineableInterface> * waiting_addr;
                std::binary_semaphore * smp;
            };

            std::deque<std::shared_ptr<CoroutineableInterface>> coroutineable_vec;
            std::vector<PriorityBucket> priority_vec;
            std::deque<WaitBucket> wait_bucket_vec;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            bool was_poisoned;

        public:
            
            CoroutineableManager(): coroutineable_vec(),
                                    priority_vec(),
                                    wait_bucket_vec(),
                                    mtx(fair_mutex::make_unique_fair_atomic_flag()),
                                    was_poisoned(false){}

            void add(std::shared_ptr<CoroutineableInterface> coroutineable,
                     std::optional<std::chrono::time_point<std::chrono::system_clock>> wakeup_time) noexcept
            {
                if (coroutineable == nullptr)
                {
                    return;
                }

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->was_poisoned)
                    {
                        return;
                    }

                    if (!wakeup_time.has_value())
                    {
                        if (!this->wait_bucket_vec.empty())
                        {
                            *wait_bucket_vec.front().waiting_addr = std::move(coroutineable);
                            wait_bucket_vec.front().smp->release();

                            return;
                        }

                        this->coroutineable_vec.push_back(std::move(coroutineable));
                        return;
                    }

                    auto greater = [](const auto& lhs, const auto& rhs)
                    {
                        return lhs.wakeup_time > rhs.wakeup_time;
                    };

                    this->priority_vec.push_back(PriorityBucket{.item = std::move(coroutineable), .wakeup_time = wakeup_time.value()});
                    std::push_heap(this->priority_vec.begin(), this->priority_vec.end(), greater);
                }
            }

            auto get() noexcept -> std::shared_ptr<CoroutineableInterface>
            {
                std::shared_ptr<CoroutineableInterface> waiting_item{};
                std::binary_semaphore smp(0);

                {

                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->was_poisoned)
                    {
                        return nullptr;
                    }

                    if (!this->coroutineable_vec.empty())
                    {
                        auto rs = std::move(this->coroutineable_vec.front());
                        this->coroutineable_vec.pop_front();

                        return rs;
                    }

                    this->wait_bucket_vec.push_back(WaitBucket
                    {
                        .waiting_addr   = &waiting_item,
                        .smp            = &smp
                    });
                }

                smp.acquire();
                return waiting_item;
            }

            void update() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_poisoned)
                {
                    return;
                }

                auto greater = [](const auto& lhs, const auto& rhs)
                {
                    return lhs.wakeup_time > rhs.wakeup_time;
                };

                std::chrono::time_point<std::chrono::system_clock> bar = std::chrono::system_clock::now();

                while (true)
                {
                    if (this->priority_vec.empty())
                    {
                        return;
                    }

                    if (this->priority_vec.front().wakeup_time >= bar)
                    {
                        return;
                    }

                    std::shared_ptr<CoroutineableInterface> item = std::move(this->priority_vec.front().item);
                    std::pop_heap(this->priority_vec.begin(), this->priority_vec.end(), greater);
                    this->priority_vec.pop_back();

                    if (!this->wait_bucket_vec.empty())
                    {
                        *wait_bucket_vec.front().waiting_addr = std::move(item);
                        wait_bucket_vec.front().smp->release();

                        continue;
                    }

                    this->coroutineable_vec.push_back(std::move(item));
                }
            }

            void poison() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_poisoned)
                {
                    return;
                }

                for (const auto& waitable: this->wait_bucket_vec)
                {
                    *waitable.waiting_addr = nullptr;
                    waitable.smp->release();
                }

                this->wait_bucket_vec.clear();
                this->was_poisoned = true;
            }
    };

    class Worker
    {
        private:

            std::atomic<bool> poison_pill;
            std::shared_ptr<CoroutineableManagerInterface> manager;
            std::atomic<bool> run_broke_pill;

            static inline constexpr size_t POISON_CHK_INTERVAL = 8u;

        public:

            Worker(std::shared_ptr<CoroutineableManagerInterface> manager)
            {
                if (manager == nullptr)
                {
                    throw std::invalid_argument("bad manager, null");
                }

                this->poison_pill       = false;
                this->manager           = manager;
                this->run_broke_pill    = false;
            }

            void run() noexcept
            {
                if (this->run_broke_pill.exchange(true, std::memory_order_relaxed))
                {
                    std::abort();
                }

                size_t local_counter = 0u;

                while (true)
                {
                    if (local_counter == POISON_CHK_INTERVAL)
                    {
                        if (this->poison_pill.load(std::memory_order_relaxed))
                        {
                            break;
                        }

                        local_counter = 0u;
                    }

                    local_counter += 1;
                    std::shared_ptr<CoroutineableInterface> coroutine = this->manager->get();

                    if (coroutine == nullptr)
                    {
                        continue;
                    }

                    if (coroutine->has_next())
                    {
                        bool need_sleep = !coroutine->next();

                        if (need_sleep)
                        {
                            using dur_t = typename std::chrono::time_point<std::chrono::system_clock>::duration;
                            std::chrono::time_point<std::chrono::system_clock> nxt_timepoint = std::chrono::time_point_cast<dur_t>(std::chrono::system_clock::now() + COROUTINE_DELAY);

                            this->manager->add(std::move(coroutine), nxt_timepoint);
                        }
                        else
                        {
                            this->manager->add(std::move(coroutine), std::nullopt);
                        }
                    }
                }
            }

            void poison() noexcept
            {
                this->poison_pill.exchange(true, std::memory_order_relaxed);
            }
    };

    struct MuscleWorkerResource
    {
        std::shared_ptr<Worker> worker;
        std::shared_ptr<std::thread> thr;
    };

    auto get_muscle_worker(std::shared_ptr<CoroutineableManagerInterface> manager) -> std::shared_ptr<void>
    {
        if (manager == nullptr)
        {
            throw std::invalid_argument("bad manager, null");
        }

        auto destructor = [](void * arg) noexcept
        {
            MuscleWorkerResource * worker_pack = static_cast<MuscleWorkerResource *>(arg); //safe memory access, by shared_ptr<> acquisition of arg when reaches 0 only, I'm afraid that std does not understand what they wrote
            worker_pack->worker->poison(); //poison fine, we are holding right address, and followed by a mutex acquisition
            std::atomic_signal_fence(std::memory_order_seq_cst);
            worker_pack->thr->join(); //join fine, we are holding right memory reference due to the shared_ptr<> and we are the sole operator of the std::thread

            delete worker_pack;
        };

        std::shared_ptr<Worker> worker      = std::make_shared<Worker>(std::move(manager));
        auto worker_runner = [=]() noexcept
        {
            worker->run();
        };

        std::shared_ptr<std::thread> thr    = std::make_shared<std::thread>(worker_runner);
        MuscleWorkerResource * worker_pack;

        try
        {
            worker_pack = new MuscleWorkerResource
            {
                MuscleWorkerResource
                {
                    .worker = worker,
                    .thr    = thr
                }
            };
        }
        catch (...)
        {
            std::abort();
        }

        return std::unique_ptr<void, decltype(destructor)>
        (
            static_cast<void * >(worker_pack),
            destructor
        );
    }

    class PeriodicUpdateWorker
    {
        private:

            std::shared_ptr<UpdatableInterface> updatable;
            std::chrono::nanoseconds periodic_dur;
            std::atomic<bool> poison_pill;
            std::atomic<bool> run_broke_pill;

            static inline constexpr size_t POISON_CHK_INTERVAL = 8u;

        public:

            PeriodicUpdateWorker(std::shared_ptr<UpdatableInterface> updatable,
                                 std::chrono::nanoseconds periodic_dur = COROUTINE_DELAY)
            {
                if (updatable == nullptr)
                {
                    throw std::invalid_argument("bad updatable, null");
                }

                if (periodic_dur < std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad temporal duration, negative");
                }

                this->updatable         = std::move(updatable);
                this->periodic_dur      = periodic_dur;
                this->poison_pill       = false;
                this->run_broke_pill    = false;
            }

            void run() noexcept
            {
                if (this->run_broke_pill.exchange(true, std::memory_order_relaxed))
                {
                    std::abort();
                }

                size_t local_counter = 0u;

                while (true)
                {
                    if (local_counter == POISON_CHK_INTERVAL)
                    {
                        if (this->poison_pill.load(std::memory_order_relaxed))
                        {
                            break;
                        }

                        local_counter = 0u;
                    }

                    local_counter += 1u;

                    this->updatable->update();
                    stdx::high_resolution_sleep(this->periodic_dur);
                }
            }

            void poison() noexcept
            {
                this->poison_pill.exchange(true, std::memory_order_relaxed);
            }
    };

    struct PeriodicUpdateWorkerResource
    {
        std::shared_ptr<PeriodicUpdateWorker> worker;
        std::shared_ptr<std::thread> thr;
    };

    auto get_periodic_update_worker(std::shared_ptr<CoroutineableManagerInterface> manager) -> std::shared_ptr<void>
    {
        if (manager == nullptr)
        {
            throw std::invalid_argument("bad manager, null");
        }

        auto destructor = [](void * arg) noexcept
        {
            PeriodicUpdateWorkerResource * worker_pack = static_cast<PeriodicUpdateWorkerResource *>(arg); //safe memory access, by shared_ptr<> acquisition of arg when reaches 0 only, I'm afraid that std does not understand what they wrote
            worker_pack->worker->poison(); //poison fine, we are holding right address, and followed by a mutex acquisition
            std::atomic_signal_fence(std::memory_order_seq_cst);
            worker_pack->thr->join(); //join fine, we are holding right memory reference due to the shared_ptr<> and we are the sole operator of the std::thread

            delete worker_pack;
        };

        std::shared_ptr<PeriodicUpdateWorker> worker = std::make_shared<PeriodicUpdateWorker>(std::move(manager));
        auto worker_runner = [=]() noexcept
        {
            worker->run();
        };

        std::shared_ptr<std::thread> thr    = std::make_shared<std::thread>(worker_runner);
        PeriodicUpdateWorkerResource * worker_pack;

        try
        {
            worker_pack = new PeriodicUpdateWorkerResource
            {
                PeriodicUpdateWorkerResource
                {
                    .worker = worker,
                    .thr    = thr
                }
            };
        }
        catch (...)
        {
            std::abort();
        }

        return std::unique_ptr<void, decltype(destructor)>
        (
            static_cast<void * >(worker_pack),
            destructor
        );
    }

    auto get_container(std::vector<std::shared_ptr<void>>&& container) -> std::shared_ptr<void>
    {
        return std::make_shared<std::vector<std::shared_ptr<void>>>(std::move(container));
    }

    class Launcher: public virtual LauncherInterface
    {
        private:

            std::shared_ptr<CoroutineableManagerInterface> manager;
            std::shared_ptr<void> daemon;
        
        public:

            Launcher(std::shared_ptr<CoroutineableManagerInterface> manager,
                     std::shared_ptr<void> daemon) noexcept: manager(std::move(manager)),
                                                             daemon(std::move(daemon)){}

            ~Launcher() noexcept
            {
                this->manager->poison();
                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->daemon = nullptr;
            }

            void add(std::shared_ptr<CoroutineableInterface> coroutineable) noexcept
            {
                this->manager->add(std::move(coroutineable), std::nullopt);
            }
    };

    class LauncherFactory
    {
        public:

            static auto get_normal_launcher() -> std::unique_ptr<LauncherInterface>
            {
                try
                {
                    std::shared_ptr<CoroutineableManagerInterface> manager = std::make_shared<CoroutineableManager>();
                    std::shared_ptr<void> muscle_worker = get_muscle_worker(manager);
                    std::shared_ptr<void> periodic_update_worker = get_periodic_update_worker(manager);

                    return std::make_unique<Launcher>(manager, get_container({muscle_worker, periodic_update_worker}));
                }
                catch (...)
                {
                    std::abort();
                }
            }
    };

    class WaitingCoroutineableWrapper: public virtual CoroutineableInterface
    {
        private:

            std::shared_ptr<CoroutineableInterface> base;
            std::shared_ptr<std::atomic<bool>> complete_status;

        public:

            WaitingCoroutineableWrapper(std::shared_ptr<CoroutineableInterface> base,
                                        std::shared_ptr<std::atomic<bool>> complete_status) noexcept: base(std::move(base)),
                                                                                                      complete_status(std::move(complete_status)){}

            auto next() noexcept -> bool
            {
                //this is hard, according to our ownership model, the one that holds reference to the coroutineable must have the memory references of the coroutineable in the local pool
                return this->base->next();
            }

            auto has_next() noexcept -> bool
            {
                if (this->base->has_next())
                {
                    return true;
                }

                this->complete_status->exchange(true, std::memory_order_release);
                this->complete_status->notify_all();

                return false;
            }
    };

    class CoroutineWaiter
    {
        private:

            std::shared_ptr<std::atomic<bool>> complete_status;

        public:

            CoroutineWaiter(std::shared_ptr<std::atomic<bool>> complete_status) noexcept: complete_status(std::move(complete_status)){}

            inline __attribute__((always_inline)) auto is_completed() noexcept -> bool
            {
                if (!this->complete_status->load(std::memory_order_relaxed))
                {
                    return false;
                }

                std::atomic_thread_fence(std::memory_order_acquire);
                return true;
            }

            inline __attribute__((always_inline)) void wait() noexcept
            {
                this->complete_status->wait(false, std::memory_order_acquire);
            }
    };
    
    struct Signature{};
    struct Signature1{};
    struct Signature2{};

    using NetworkLauncherSingleton  = stdx::singleton_container<std::shared_ptr<LauncherInterface>, Signature>;
    using FileIOLauncherSingleton   = stdx::singleton_container<std::shared_ptr<LauncherInterface>, Signature1>;
    using ComputeLauncherSingleton  = stdx::singleton_container<std::shared_ptr<LauncherInterface>, Signature2>;

    static inline constexpr uint8_t NETWORK_COROUTINE   = 0u;
    static inline constexpr uint8_t FILEIO_COROUTINE    = 1u;
    static inline constexpr uint8_t COMPUTE_COROUTINE   = 2u;

    void init(bool has_network_coroutine_machine = true,
              bool has_fileio_coroutine_machine = true,
              bool has_compute_coroutine_machine = true)
    {
        if (has_network_coroutine_machine)
        {
            NetworkLauncherSingleton::get() = LauncherFactory::get_normal_launcher();
        }

        if (has_fileio_coroutine_machine)
        {
            FileIOLauncherSingleton::get()  = LauncherFactory::get_normal_launcher();
        }

        if (has_compute_coroutine_machine)
        {
            ComputeLauncherSingleton::get() = LauncherFactory::get_normal_launcher();
        }

        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void deinit() noexcept
    {
        std::atomic_thread_fence(std::memory_order_seq_cst);

        NetworkLauncherSingleton::get() = nullptr;
        FileIOLauncherSingleton::get()  = nullptr;
        ComputeLauncherSingleton::get() = nullptr;
    }

    void run_detached(std::shared_ptr<CoroutineableInterface> coroutineable, uint8_t coroutine_topic)
    {
        if (coroutineable == nullptr)
        {
            return;
        }

        switch (coroutine_topic)
        {
            case NETWORK_COROUTINE:
            {
                if (NetworkLauncherSingleton::get() == nullptr)
                {
                    throw std::runtime_error("network coroutine launcher is not initialized");
                }

                NetworkLauncherSingleton::get()->add(std::move(coroutineable));
                break;
            }
            case FILEIO_COROUTINE:
            {
                if (FileIOLauncherSingleton::get() == nullptr)
                {
                    throw std::runtime_error("fileio coroutine launcher is not initialized");
                }

                FileIOLauncherSingleton::get()->add(std::move(coroutineable));
                break;
            }
            case COMPUTE_COROUTINE:
            {
                if (ComputeLauncherSingleton::get() == nullptr)
                {
                    throw std::runtime_error("compute coroutine launcher is not initialized");
                }

                ComputeLauncherSingleton::get()->add(std::move(coroutineable));
                break;
            }
            default:
            {
                throw std::invalid_argument("bad coroutine topic, enumeration out of range");
            }
        }
    }

    auto run_promise(std::shared_ptr<CoroutineableInterface> coroutineable, uint8_t coroutine_topic) -> CoroutineWaiter
    {
        if (coroutineable == nullptr)
        {
            throw std::invalid_argument("bad coroutineable, null");
        }

        std::shared_ptr<std::atomic<bool>> complete_status              = std::make_shared<std::atomic<bool>>(false);
        std::shared_ptr<CoroutineableInterface> promoted_coroutineable  = std::make_shared<WaitingCoroutineableWrapper>(coroutineable, complete_status);

        run_detached(std::move(promoted_coroutineable), coroutine_topic);

        return CoroutineWaiter(complete_status);
    }
}

#endif