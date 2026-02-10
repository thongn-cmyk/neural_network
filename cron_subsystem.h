#ifndef __CRON_SUBSYSTEM_H__
#define __CRON_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <memory>
#include "logging_subsystem.h"
#include <semaphore>
#include <functional>
#include <utility>
#include <type_traits>

namespace cron_subsystem
{
    //point is we have a dedicated waker, because we cant deep dive into the operating system and their friends
    //this dedicated waiter would fulfill the punctuality of the contracts which we'd leverage for other wakeup tasks there forward

    static inline constexpr size_t CRON_SCAN_FREQUENCY                  = 10000u;
    static inline const std::chrono::nanoseconds CRON_SCAN_WAVELENGTH   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1) / CRON_SCAN_FREQUENCY);

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

    struct PeriodicCronJob
    {
        std::shared_ptr<UpdatableInterface> updatable;
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

            virtual void launch(PeriodicCronJob&& cron_job) = 0;
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
                        cron_job.updatable->update();
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

            void launch(PeriodicCronJob&& cron_job)
            {
                this->cron_container->push(std::move(cron_job));
            }
    };

    void init()
    {

    }

    void deinit() noexcept
    {

    }

    auto register_periodic_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                   std::chrono::nanoseconds dur) -> std::shared_ptr<void>
    {
        return {};
    }

    void register_fire_and_forget_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                          std::chrono::time_point<std::chrono::system_clock> timepoint)
    {
        return;
    }
}

#endif