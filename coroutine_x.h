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
#include "cron_subsystem.h"

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

    //let's aim for the obvious, next-to-base non recursive solution for this particular coroutine_x
    //we'd have to solve for the otherwise in other components
    //the waiting tiem cannot be 100 microseconds, because it'd explode, so we'd have to add another base2 factor or accumulative factor to scale it to ... n(n+1)/2, such is that latency n for n(n+1)/2 task

    static inline const std::chrono::nanoseconds COROUTINE_DELAY = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(100));

    class CoroutineableInterface
    {
        public:

            virtual ~CoroutineableInterface() noexcept = default;

            virtual auto has_next() noexcept -> bool = 0;
            virtual auto next() noexcept -> bool = 0;
    };

    class CoroutineableManagerInterface: public virtual cron_subsystem::UpdatableInterface
    {
        public:

            virtual ~CoroutineableManagerInterface() noexcept = default;

            virtual void add(std::shared_ptr<CoroutineableInterface> coroutineable,
                             std::optional<std::chrono::time_point<std::chrono::system_clock>> wakeup_time) noexcept = 0;

            virtual auto get() noexcept -> std::shared_ptr<CoroutineableInterface> = 0;
            virtual void poison() noexcept = 0;
    };

    class ConsecutiveDelayCalculatorInterface
    {
        public:

            virtual ~ConsecutiveDelayCalculatorInterface() noexcept = default;

            virtual void add_delay(const std::shared_ptr<void>& object_reference) = 0;
            virtual auto get_delay(const std::shared_ptr<void>& object_reference) -> std::chrono::nanoseconds = 0;
            virtual void clear_delay(const std::shared_ptr<void>& object_reference) noexcept = 0; 
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

            auto get_priority_bucket_comparator()
            {
                auto greater = [](const PriorityBucket& lhs, const PriorityBucket& rhs)
                {
                    return lhs.wakeup_time > rhs.wakeup_time;
                };

                return greater;
            }

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
                            *this->wait_bucket_vec.front().waiting_addr = std::move(coroutineable);
                            this->wait_bucket_vec.front().smp->release();
                            this->wait_bucket_vec.pop_front();

                            return;
                        }

                        this->coroutineable_vec.push_back(std::move(coroutineable));
                        return;
                    }

                    this->priority_vec.push_back
                    (
                        PriorityBucket
                        {
                            .item           = std::move(coroutineable),
                            .wakeup_time    = wakeup_time.value()
                        }
                    );

                    std::push_heap(this->priority_vec.begin(), this->priority_vec.end(), this->get_priority_bucket_comparator());
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

                    this->wait_bucket_vec.push_back
                    (
                        WaitBucket
                        {
                            .waiting_addr   = &waiting_item,
                            .smp            = &smp
                        }
                    );
                }

                smp.acquire();
                return waiting_item;
            }

            void update()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_poisoned)
                {
                    return;
                }


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
                    std::pop_heap(this->priority_vec.begin(), this->priority_vec.end(), this->get_priority_bucket_comparator());
                    this->priority_vec.pop_back();

                    if (!this->wait_bucket_vec.empty())
                    {
                        *this->wait_bucket_vec.front().waiting_addr = std::move(item);
                        this->wait_bucket_vec.front().smp->release();
                        this->wait_bucket_vec.pop_front();

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

    class Base2ConsecutiveDelayCalculator: public virtual ConsecutiveDelayCalculatorInterface
    {
        private:

            struct ReferenceBucket
            {
                std::shared_ptr<void> obj;
                size_t counter;
            };

            std::unordered_map<uintptr_t, ReferenceBucket> bucket_map;
            std::chrono::nanoseconds max_delay;
            std::chrono::nanoseconds base_multiplier;

            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

            static inline constexpr size_t BASE = 2u;
            static inline constexpr size_t MAX_BASE_COUNTER = 30u;
        
        public:
            
            Base2ConsecutiveDelayCalculator(std::chrono::nanoseconds max_delay,
                                            std::chrono::nanoseconds base_multiplier)
            {
                if (max_delay < std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad max delay, negative value");
                }

                if (base_multiplier < std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad base multiplier, negative value");
                }

                this->bucket_map        = {};
                this->max_delay         = max_delay;
                this->base_multiplier   = base_multiplier;
                this->mtx               = fair_mutex::make_unique_fair_atomic_flag();
            }

            void add_delay(const std::shared_ptr<void>& object_reference)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                uintptr_t obj_addr  = reinterpret_cast<uintptr_t>(object_reference.get());
                auto map_ptr        = this->bucket_map.find(obj_addr);

                if (map_ptr == this->bucket_map.end())
                {
                    auto [new_map_ptr, status] = this->bucket_map.insert
                    (
                        {
                            obj_addr, 
                            ReferenceBucket
                            {
                                .obj        = object_reference, 
                                .counter    = size_t{0u}
                            }
                        }
                    );

                    map_ptr = new_map_ptr;
                }

                map_ptr->second.counter += 1u;
            }

            auto get_delay(const std::shared_ptr<void>& object_reference) -> std::chrono::nanoseconds
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                uintptr_t obj_addr  = reinterpret_cast<uintptr_t>(object_reference.get());
                size_t counter;

                if (auto map_ptr = this->bucket_map.find(obj_addr); map_ptr != this->bucket_map.end())
                {
                    counter = map_ptr->second.counter;
                }
                else
                {
                    counter = 0u;
                }

                counter = std::min(counter, MAX_BASE_COUNTER);

                std::chrono::nanoseconds tentative_delay    = std::chrono::duration_cast<std::chrono::nanoseconds>(this->base_multiplier * static_cast<size_t>(std::pow(BASE, counter)));
                std::chrono::nanoseconds actual_delay       = std::clamp(tentative_delay, std::chrono::nanoseconds(0), this->max_delay);

                return actual_delay;
            }

            void clear_delay(const std::shared_ptr<void>& object_reference) noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                uintptr_t obj_addr  = reinterpret_cast<uintptr_t>(object_reference.get());
                this->bucket_map.erase(obj_addr);
            }
    };

    class Worker
    {
        private:

            std::atomic<bool> poison_pill;
            std::shared_ptr<CoroutineableManagerInterface> manager;
            std::shared_ptr<ConsecutiveDelayCalculatorInterface> delay_calculator;
            std::atomic<bool> run_broke_pill;

            static inline constexpr size_t POISON_CHK_INTERVAL = 8u;

        public:

            Worker(std::shared_ptr<CoroutineableManagerInterface> manager,
                   std::shared_ptr<ConsecutiveDelayCalculatorInterface> delay_calculator)
            {
                if (manager == nullptr)
                {
                    throw std::invalid_argument("bad manager, null");
                }

                if (delay_calculator == nullptr)
                {
                    throw std::invalid_argument("bad delay calculator, null");
                }

                this->poison_pill       = false;
                this->manager           = manager;
                this->delay_calculator  = delay_calculator;
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

                            std::chrono::nanoseconds delay_time = this->delay_calculator->get_delay(coroutine);
                            this->delay_calculator->add_delay(coroutine);
                            std::chrono::time_point<std::chrono::system_clock> nxt_timepoint = std::chrono::time_point_cast<dur_t>(std::chrono::system_clock::now() + delay_time);

                            this->manager->add(coroutine, nxt_timepoint);
                        }
                        else
                        {
                            this->delay_calculator->clear_delay(coroutine);
                            this->manager->add(coroutine, std::nullopt);
                        }
                    }
                    else
                    {
                        this->delay_calculator->clear_delay(coroutine);
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

    auto get_muscle_worker(std::shared_ptr<CoroutineableManagerInterface> manager,
                           std::shared_ptr<ConsecutiveDelayCalculatorInterface> delay_calculator) -> std::shared_ptr<void>
    {
        auto destructor = [](void * arg) noexcept
        {
            MuscleWorkerResource * worker_pack = static_cast<MuscleWorkerResource *>(arg); //safe memory access, by shared_ptr<> acquisition of arg when reaches 0 only, I'm afraid that std does not understand what they wrote
            worker_pack->worker->poison(); //poison fine, we are holding right address, and followed by a mutex acquisition
            std::atomic_signal_fence(std::memory_order_seq_cst);
            worker_pack->thr->join(); //join fine, we are holding right memory reference due to the shared_ptr<> and we are the sole operator of the std::thread

            delete worker_pack;
        };

        std::shared_ptr<Worker> worker      = std::make_shared<Worker>(manager, delay_calculator);
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

    auto get_periodic_update_worker(std::shared_ptr<CoroutineableManagerInterface> manager,
                                    std::chrono::nanoseconds periodic_dur = COROUTINE_DELAY) -> std::shared_ptr<void>
    {
        return cron_subsystem::register_periodic_cronjob(manager, periodic_dur);
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
        private:

            static auto get_shared_daemon_container(std::vector<std::shared_ptr<void>>&& container) -> std::shared_ptr<void>
            {
                return std::make_shared<std::vector<std::shared_ptr<void>>>(std::move(container));
            }

        public:

            static auto get_normal_launcher() -> std::unique_ptr<LauncherInterface>
            {
                std::chrono::nanoseconds MAX_TASK_DELAY     = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(100));
                std::chrono::nanoseconds BASE_TASK_DELAY    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(10));

                try
                {
                    std::shared_ptr<CoroutineableManagerInterface> manager                  = std::make_shared<CoroutineableManager>();
                    std::shared_ptr<ConsecutiveDelayCalculatorInterface> delay_calculator   = std::make_shared<Base2ConsecutiveDelayCalculator>(MAX_TASK_DELAY, BASE_TASK_DELAY);
                    std::shared_ptr<void> muscle_worker                                     = get_muscle_worker(manager, delay_calculator);
                    std::shared_ptr<void> periodic_update_worker                            = get_periodic_update_worker(manager);

                    return std::make_unique<Launcher>(manager, get_shared_daemon_container({muscle_worker, periodic_update_worker}));
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

                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    this->complete_status->exchange(true, std::memory_order_seq_cst);
                }
                else
                {
                    this->complete_status->exchange(true, std::memory_order_release);
                }

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

                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }
                else
                {
                    std::atomic_thread_fence(std::memory_order_acquire);
                }

                return true;
            }

            inline __attribute__((always_inline)) void wait() noexcept
            {
                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    this->complete_status->wait(false, std::memory_order_seq_cst);
                }
                else
                {
                    this->complete_status->wait(false, std::memory_order_acquire);
                }
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