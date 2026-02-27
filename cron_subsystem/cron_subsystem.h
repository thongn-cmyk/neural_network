#ifndef __CRON_SUBSYSTEM_H__
#define __CRON_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <memory>
#include <logging_subsystem/logging_subsystem.h>
#include <semaphore>
#include <functional>
#include <utility>
#include <type_traits>
#include <deque>

namespace cron_subsystem
{
    //point is we have a dedicated waker, because we cant deep dive into the operating system and their friends
    //this dedicated waiter would fulfill the punctuality of the contracts which we'd leverage for other wakeup tasks there forward

    static inline const std::chrono::nanoseconds PERIODIC_CRON_SCAN_WAVELENGTH  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(100));
    static inline constexpr size_t PERIODIC_CRON_WORKER_SZ                      = 1;
    static inline constexpr bool PERIODIC_CRON_IS_SLEEPLESS_WAKER               = false;

    class NoexceptUpdatableInterface
    {
        public:

            virtual ~NoexceptUpdatableInterface() noexcept = default;

            virtual void update() noexcept = 0;
    };

    class UpdatableInterface
    {
        public:

            virtual ~UpdatableInterface() noexcept = default;

            virtual void update() = 0;
    };

    class InternalUpdatableInterface
    {
        public:

            virtual ~InternalUpdatableInterface() noexcept = default;

            virtual auto update() -> bool = 0;
    };

    struct PeriodicCronJob
    {
        std::shared_ptr<InternalUpdatableInterface> updatable;
        std::chrono::nanoseconds dur;
    };

    class PeriodicCronContainerInterface: public virtual NoexceptUpdatableInterface
    {
        public:

            virtual ~PeriodicCronContainerInterface() noexcept = default;

            virtual void push(PeriodicCronJob&& cron_job) = 0;
            virtual auto pop() noexcept -> PeriodicCronJob = 0;
            virtual void poison() noexcept = 0;
    };

    class SleepingMachineInterface
    {
        public:

            virtual ~SleepingMachineInterface() noexcept = default;

            virtual void sleep_for(std::chrono::nanoseconds dur) noexcept = 0;
    };

    class PeriodicCronLauncherInterface
    {
        public:

            virtual ~PeriodicCronLauncherInterface() noexcept = default;

            virtual auto launch(const std::shared_ptr<UpdatableInterface>& cron_job,
                                std::chrono::nanoseconds dur) -> std::shared_ptr<void> = 0;
    };

    class BusySleeper: public virtual SleepingMachineInterface
    {
        private:

            static inline constexpr size_t CRON_CHECK_REVOLUTION_SZ = size_t{1} << 18;

        public:

            void sleep_for(std::chrono::nanoseconds dur) noexcept
            {
                if (dur < std::chrono::nanoseconds(0))
                {
                    return;
                }

                size_t counter = 0u;

                std::chrono::time_point<std::chrono::system_clock> since    = std::chrono::system_clock::now();
                std::chrono::time_point<std::chrono::system_clock> now      = since;

                while (true)
                {
                    if (counter == CRON_CHECK_REVOLUTION_SZ)
                    {
                        now = std::chrono::system_clock::now();
                        std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - since);

                        if (lapsed >= dur)
                        {
                            return;
                        }

                        counter = 0u;
                    }

                    counter += 1u;
                }
            }
    };

    class StdSleeper: public virtual SleepingMachineInterface
    {
        public:

            void sleep_for(std::chrono::nanoseconds dur) noexcept
            {
                std::this_thread::sleep_for(dur);
            }
    };

    class PeriodicUpdateWorker
    {
        private:

            std::shared_ptr<NoexceptUpdatableInterface> updatable;
            std::unique_ptr<SleepingMachineInterface> sleeping_machine;
            std::chrono::nanoseconds periodic_dur;
            std::atomic<bool> poison_pill;
            std::atomic<bool> run_broke_pill;

            static inline constexpr size_t POISON_CHK_INTERVAL = 8u;

        public:

            PeriodicUpdateWorker(std::shared_ptr<NoexceptUpdatableInterface> updatable,
                                 std::chrono::nanoseconds periodic_dur,
                                 std::unique_ptr<SleepingMachineInterface> sleeping_machine)
            {
                if (updatable == nullptr)
                {
                    throw std::invalid_argument("bad updatable, null");
                }

                if (periodic_dur < std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad temporal duration, negative");
                }

                if (sleeping_machine == nullptr)
                {
                    throw std::invalid_argument("bad sleeping machine, null");
                }

                this->updatable         = std::move(updatable);
                this->sleeping_machine  = std::move(sleeping_machine);
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
                    this->sleeping_machine->sleep_for(this->periodic_dur);
                }
            }

            void poison() noexcept
            {
                this->poison_pill.exchange(true, std::memory_order_relaxed);
            }
    };

    struct WakerWorkerResource
    {
        std::shared_ptr<PeriodicUpdateWorker> worker;
        std::shared_ptr<std::thread> thr;
    };

    auto get_periodic_waker_worker(const std::shared_ptr<NoexceptUpdatableInterface>& updatable,
                                   std::unique_ptr<SleepingMachineInterface>&& sleeping_machine,
                                   std::chrono::nanoseconds periodic_dur) -> std::shared_ptr<void>
    {
        auto destructor = [](void * arg) noexcept
        {
            WakerWorkerResource * worker_resource = static_cast<WakerWorkerResource *>(arg);
            worker_resource->worker->poison();
            std::atomic_signal_fence(std::memory_order_seq_cst);
            worker_resource->thr->join();

            delete worker_resource;
        };

        std::shared_ptr<PeriodicUpdateWorker> worker = std::make_shared<PeriodicUpdateWorker>(updatable,
                                                                                              periodic_dur,
                                                                                              std::move(sleeping_machine));

        auto worker_runner = [=]() noexcept
        {
            worker->run();
        };

        std::shared_ptr<std::thread> thr = std::make_shared<std::thread>(worker_runner);
        WakerWorkerResource * worker_resource;

        try
        {
            worker_resource = new WakerWorkerResource
            {
                WakerWorkerResource
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
            static_cast<void * >(worker_resource),
            destructor
        );
    }

    class PeriodicCronContainer: public virtual PeriodicCronContainerInterface
    {
        private:

            struct WaitBucket
            {
                PeriodicCronJob * waiting_addr;
                std::binary_semaphore * smp;
            };

            struct CronBucket
            {
                PeriodicCronJob base_job;
                std::chrono::time_point<std::chrono::system_clock> expiry;
            };

            std::vector<CronBucket> periodic_cron_bucket_vec;
            std::deque<WaitBucket> wait_bucket_vec;
            bool was_poisoned;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            
            auto get_periodic_cron_bucket_comparator()
            {
                auto greater = [](const CronBucket& lhs, const CronBucket& rhs)
                {
                    return lhs.expiry > rhs.expiry;
                };

                return greater;
            }

        public:

            PeriodicCronContainer(): periodic_cron_bucket_vec(),
                                     wait_bucket_vec(),
                                     was_poisoned(false),
                                     mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void push(PeriodicCronJob&& cron_job)
            {
                if (cron_job.updatable == nullptr)
                {
                    throw std::invalid_argument("bad cron job, null");
                }

                if (cron_job.dur < std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad cron job duration, negative");
                }

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                
                if (this->was_poisoned)
                {
                    throw std::runtime_error("bad operation, container was poisoned");
                }

                auto dur = cron_job.dur;

                try
                {
                    this->periodic_cron_bucket_vec.push_back
                    (
                        CronBucket
                        {
                            .base_job   = std::move(cron_job),
                            .expiry     = std::chrono::time_point_cast<typename std::chrono::time_point<std::chrono::system_clock>::duration>(std::chrono::system_clock::now() + dur)
                        }
                    );
                }
                catch (...)
                {
                    std::abort();
                }

                std::push_heap(this->periodic_cron_bucket_vec.begin(), this->periodic_cron_bucket_vec.end(), this->get_periodic_cron_bucket_comparator());
            }

            auto pop() noexcept -> PeriodicCronJob
            {
                PeriodicCronJob waiting_item{};
                std::binary_semaphore waiting_smp(0);

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->was_poisoned)
                    {
                        return PeriodicCronJob
                        {
                            .updatable  = {},
                            .dur        = {}
                        };
                    }

                    if (!this->periodic_cron_bucket_vec.empty())
                    {
                        if (this->periodic_cron_bucket_vec.front().expiry < std::chrono::system_clock::now())
                        {
                            auto item = std::move(this->periodic_cron_bucket_vec.front().base_job);
                            std::pop_heap(this->periodic_cron_bucket_vec.begin(), this->periodic_cron_bucket_vec.end(), this->get_periodic_cron_bucket_comparator());
                            this->periodic_cron_bucket_vec.pop_back();

                            return item;
                        }
                    }

                    this->wait_bucket_vec.push_back
                    (
                        WaitBucket
                        {
                            .waiting_addr = &waiting_item,
                            .smp = &waiting_smp
                        }
                    );
                }

                waiting_smp.acquire();

                return waiting_item;
            }

            void update() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_poisoned)
                {
                    return;
                }

                auto bar = std::chrono::system_clock::now();

                while (true)
                {
                    if (this->periodic_cron_bucket_vec.empty())
                    {
                        return;
                    }

                    if (this->wait_bucket_vec.empty())
                    {
                        return;
                    }

                    if (this->periodic_cron_bucket_vec.front().expiry >= bar)
                    {
                        return;
                    }

                    auto waitable   = std::move(this->wait_bucket_vec.front());
                    this->wait_bucket_vec.pop_front();
                    auto fetchable  = std::move(this->periodic_cron_bucket_vec.front().base_job);

                    std::pop_heap(this->periodic_cron_bucket_vec.begin(), this->periodic_cron_bucket_vec.end(), this->get_periodic_cron_bucket_comparator());
                    this->periodic_cron_bucket_vec.pop_back();

                    *waitable.waiting_addr = std::move(fetchable);
                    waitable.smp->release();
                }
            }

            void poison() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (std::exchange(this->was_poisoned, true))
                {
                    return;
                }

                for (const auto& wait_bucket: this->wait_bucket_vec)
                {
                    *wait_bucket.waiting_addr = {};
                    wait_bucket.smp->release();
                }
            }
    };

    class CronWorker
    {
        private:

            std::shared_ptr<PeriodicCronContainerInterface> workorder_container;
            std::atomic<bool> poison_pill;
            std::atomic<bool> run_broke_pill;

            static inline constexpr size_t POISON_CHK_INTERVAL = 8u;

        public:

            CronWorker(std::shared_ptr<PeriodicCronContainerInterface> workorder_container)
            {
                if (workorder_container == nullptr)
                {
                    throw std::invalid_argument("bad workorder container, null");
                }

                this->workorder_container   = std::move(workorder_container);
                this->poison_pill           = false;
                this->run_broke_pill        = false;
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
                    PeriodicCronJob cron_job = this->workorder_container->pop();

                    if (cron_job.updatable == nullptr)
                    {
                        continue;
                    }

                    try
                    {
                        bool has_next = cron_job.updatable->update();

                        if (!has_next)
                        {
                            continue;
                        }
                    }
                    catch (...)
                    {
                        logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("cron_subsystem")
                                                                                       .topic("CronWorker")
                                                                                       .topic("run cron job")
                                                                                       .error()
                                                                                       .message(std::current_exception())
                                                                                       .get());
                    }

                    try
                    {
                        this->workorder_container->push(std::move(cron_job));
                    }
                    catch (...)
                    {
                        logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("cron_subsystem")
                                                                                       .topic("CronWorker")
                                                                                       .topic("re-queue cron job")
                                                                                       .critical()
                                                                                       .message(std::current_exception())
                                                                                       .get());
                    }
                }
            }

            void poison() noexcept
            {
                this->poison_pill.exchange(true, std::memory_order_relaxed);
            }
    };

    struct CronWorkerResource
    {
        std::shared_ptr<CronWorker> worker;
        std::shared_ptr<std::thread> thr;
    };

    auto get_periodic_cron_worker(const std::shared_ptr<PeriodicCronContainerInterface>& cron_container) -> std::shared_ptr<void>
    {
        auto destructor = [](void * arg) noexcept
        {
            CronWorkerResource * worker_resource = static_cast<CronWorkerResource *>(arg);
            worker_resource->worker->poison();
            std::atomic_signal_fence(std::memory_order_seq_cst);
            worker_resource->thr->join();

            delete worker_resource;
        };

        std::shared_ptr<CronWorker> worker  = std::make_shared<CronWorker>(cron_container);
        auto worker_runner = [=]() noexcept
        {
            worker->run();
        };

        std::shared_ptr<std::thread> thr    = std::make_shared<std::thread>(worker_runner);
        CronWorkerResource * worker_resource;

        try
        {
            worker_resource = new CronWorkerResource
            {
                CronWorkerResource
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
            static_cast<void * >(worker_resource),
            destructor //
        );
    }

    class PeriodicCronLauncher: public virtual PeriodicCronLauncherInterface
    {
        private:

            std::shared_ptr<PeriodicCronContainerInterface> cron_container;
            std::shared_ptr<void> daemon;
        
        public:

            PeriodicCronLauncher(std::shared_ptr<PeriodicCronContainerInterface> cron_container,
                                 std::shared_ptr<void> daemon) noexcept: cron_container(std::move(cron_container)),
                                                                         daemon(std::move(daemon)){}

            ~PeriodicCronLauncher() noexcept
            {
                this->cron_container->poison();
                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->daemon = nullptr;
            }

            auto launch(const std::shared_ptr<UpdatableInterface>& cron_job,
                        std::chrono::nanoseconds dur) -> std::shared_ptr<void>
            {
                if (cron_job == nullptr)
                {
                    throw std::invalid_argument("bad cron job, null");
                }

                if (dur < std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad cron duration, negative");
                }

                std::shared_ptr<std::atomic<bool>> poison_pill = std::make_shared<std::atomic<bool>>(false);

                auto destructor = [](std::shared_ptr<std::atomic<bool>> * poison_pill_container)
                {
                    (*poison_pill_container)->exchange(true, std::memory_order_relaxed);
                    delete poison_pill_container;
                };

                std::shared_ptr<InternalUpdatableInterface> updatable = std::make_shared<InternalUpdatableWrapper>(cron_job, poison_pill);

                this->cron_container->push
                (
                    PeriodicCronJob
                    {
                        .updatable  = updatable,
                        .dur        = dur
                    }
                );
    
                try
                {
                    return std::shared_ptr<void>(std::unique_ptr<std::shared_ptr<std::atomic<bool>>, decltype(destructor)>(new std::shared_ptr<std::atomic<bool>>(poison_pill),
                                                                                                                           destructor));

                }
                catch (...)
                {
                    std::abort();
                }
            }

        private:

            class InternalUpdatableWrapper: public virtual InternalUpdatableInterface
            {
                private:

                    std::shared_ptr<UpdatableInterface> base;
                    std::shared_ptr<std::atomic<bool>> poison_pill;
                    size_t poison_chk_interval_counter;
                    bool is_poison_confimed;

                public:

                    static inline constexpr size_t POISON_CHK_INTERVAL_DUE = size_t{1} << 0;

                    InternalUpdatableWrapper(std::shared_ptr<UpdatableInterface> base,
                                             std::shared_ptr<std::atomic<bool>> poison_pill) noexcept: base(std::move(base)),
                                                                                                       poison_pill(std::move(poison_pill)),
                                                                                                       poison_chk_interval_counter(0u),
                                                                                                       is_poison_confimed(false){}

                    auto update() -> bool
                    {
                        if (is_poison_confimed)
                        {
                            return false;
                        }

                        if (this->poison_chk_interval_counter == POISON_CHK_INTERVAL_DUE)
                        {
                            if (this->poison_pill->load(std::memory_order_relaxed))
                            {
                                this->is_poison_confimed = true;
                                return false;
                            }

                            this->poison_chk_interval_counter = 0u;
                        }

                        this->poison_chk_interval_counter += 1u;
                        this->base->update();

                        return true;
                    }
            };

    };

    class PeriodicLauncherBuilder
    {
        public:

            static inline const std::chrono::nanoseconds DEFAULT_CRON_SCAN_WAVELENGTH   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1));
            static inline const size_t DEFAULT_CONCURRENT_WORKER_SZ                     = 1u;
            static inline const bool DEFAULT_WAKER_SLEEPLESS_STATUS                     = false;

        private:

            std::chrono::nanoseconds cron_scan_wavelength;
            size_t concurrent_worker_sz;
            bool is_waker_sleepless;

        public:

            PeriodicLauncherBuilder(): cron_scan_wavelength(DEFAULT_CRON_SCAN_WAVELENGTH),
                                       concurrent_worker_sz(DEFAULT_CONCURRENT_WORKER_SZ),
                                       is_waker_sleepless(DEFAULT_WAKER_SLEEPLESS_STATUS){}

            auto set_cron_scan_wavelength(std::chrono::nanoseconds cron_scan_wavelength) -> PeriodicLauncherBuilder&
            {
                const std::chrono::nanoseconds MIN_CRON_WAVELENGTH  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(1));
                const std::chrono::nanoseconds MAX_CRON_WAVELENGTH  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));

                if (std::clamp(cron_scan_wavelength, MIN_CRON_WAVELENGTH, MAX_CRON_WAVELENGTH) != cron_scan_wavelength)
                {
                    throw std::invalid_argument("bad cron scan wavelength, out of operable range");
                }

                this->cron_scan_wavelength = cron_scan_wavelength;

                return *this;
            }

            auto set_concurrent_worker_size(size_t concurrent_worker_sz) -> PeriodicLauncherBuilder&
            {
                const size_t MIN_CONCURRENT_WORKER_SZ   = 1u;
                const size_t MAX_CONCURRENT_WORKER_SZ   = std::numeric_limits<size_t>::max();

                if (std::clamp(concurrent_worker_sz, MIN_CONCURRENT_WORKER_SZ, MAX_CONCURRENT_WORKER_SZ) != concurrent_worker_sz)
                {
                    throw std::invalid_argument("bad concurrent worker size, out of operable range");
                }

                this->concurrent_worker_sz = concurrent_worker_sz;

                return *this;
            }

            auto set_waker_kind(bool is_sleepless) -> PeriodicLauncherBuilder&
            {
                this->is_waker_sleepless = is_sleepless;

                return *this;
            }

            auto build() -> std::unique_ptr<PeriodicCronLauncherInterface>
            {
                std::shared_ptr<PeriodicCronContainerInterface> cron_container  = this->get_cron_container();
                std::shared_ptr<void> muscle_worker                             = this->get_muscle_worker(cron_container);
                std::shared_ptr<void> waker_worker                              = this->get_waker_worker(cron_container);

                return std::make_unique<PeriodicCronLauncher>(cron_container, this->get_shared_daemon_container({muscle_worker, waker_worker}));
            }

        private:

            auto get_sleeper() -> std::unique_ptr<SleepingMachineInterface>
            {
                if (this->is_waker_sleepless)
                {
                    return std::make_unique<BusySleeper>();
                }

                return std::make_unique<StdSleeper>();
            }

            auto get_shared_daemon_container(const std::vector<std::shared_ptr<void>> daemon_vec) -> std::shared_ptr<void>
            {
                return std::make_shared<std::vector<std::shared_ptr<void>>>(daemon_vec);
            }

            auto get_cron_container() -> std::unique_ptr<PeriodicCronContainerInterface>
            {
                return std::make_unique<PeriodicCronContainer>();
            }

            auto get_muscle_worker(const std::shared_ptr<PeriodicCronContainerInterface>& cron_container) -> std::shared_ptr<void>
            {
                std::vector<std::shared_ptr<void>> worker_vec{};

                for (size_t i = 0u; i < this->concurrent_worker_sz; ++i)
                {
                    worker_vec.push_back(get_periodic_cron_worker(cron_container));
                }

                return this->get_shared_daemon_container(worker_vec);
            }

            auto get_waker_worker(const std::shared_ptr<PeriodicCronContainerInterface>& cron_container) -> std::shared_ptr<void>
            {
                return get_periodic_waker_worker(cron_container, this->get_sleeper(), this->cron_scan_wavelength);
            }
    };

    struct Signature{};

    using PeriodicCronJobSingletonContainer = stdx::singleton_container<std::unique_ptr<PeriodicCronLauncherInterface>, Signature>;

    void init()
    {
        PeriodicCronJobSingletonContainer::get() = PeriodicLauncherBuilder{}.set_cron_scan_wavelength(PERIODIC_CRON_SCAN_WAVELENGTH)
                                                                            .set_concurrent_worker_size(PERIODIC_CRON_WORKER_SZ)
                                                                            .set_waker_kind(PERIODIC_CRON_IS_SLEEPLESS_WAKER)
                                                                            .build();

        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void deinit() noexcept
    {
        std::atomic_thread_fence(std::memory_order_seq_cst);

        PeriodicCronJobSingletonContainer::get() = nullptr;
    }

    auto register_periodic_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                   std::chrono::nanoseconds dur) -> std::shared_ptr<void>
    {
        if (PeriodicCronJobSingletonContainer::get() == nullptr)
        {
            std::abort();
        }

        return PeriodicCronJobSingletonContainer::get()->launch(updatable, dur);
    }

    // void register_fire_and_forget_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                        //   std::chrono::time_point<std::chrono::system_clock> timepoint)
    // {
        // return;
    // }
}

#endif