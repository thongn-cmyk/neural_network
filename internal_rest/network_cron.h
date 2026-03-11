#ifndef __NETWORK_CRON_H__
#define __NETWORK_CRON_H__

#include <cron_subsystem/cron_subsystem.h>
#include "network_concurrency.h"
#include "network_std_container.h"

namespace dg_sock::network_cron
{
    struct UpdatableInterface
    {
        public:

            virtual ~UpdatableInterface() noexcept = default;

            virtual void update() = 0;
    };

    class CronContainerInterface
    {
        public:

            virtual ~CronContainerInterface() noexcept = default;
            virtual void push(std::shared_ptr<UpdatableInterface> updatable) = 0;
            virtual auto pop() -> std::shared_ptr<UpdatableInterface> = 0;
            virtual void poison() noexcept = 0;
    };

    class HandToHandCronContainer: public virtual CronContainerInterface
    {
        private:

            struct PushArgument
            {
                std::shared_ptr<UpdatableInterface> updatable;
                std::binary_semaphore * smp;
            };

            struct PopArgument
            {
                std::shared_ptr<UpdatableInterface> * updatable;
                std::binary_semaphore * smp;
            };

            dg_sock::pow2_cyclic_queue<PushArgument> push_argument_vec;
            dg_sock::pow2_cyclic_queue<PopArgument> pop_argument_vec;
            bool is_poisoned;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;

        public:
            
            HandToHandCronContainer(): push_argument_vec(stdxx::ulog2(stdxx::ceil2(dg_sock::network_concurrency::get_thread_count()))),
                                       pop_argument_vec(stdxx::ulog2(stdxx::ceil2(dg_sock::network_concurrency::get_thread_count()))),
                                       is_poisoned(false),
                                       mtx(stdxx::make_unique_fair_atomic_flag()){}

            void push(std::shared_ptr<UpdatableInterface> updatable)
            {
                if (updatable == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::binary_semaphore smp(0);

                bool need_wait = [&]
                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->is_poisoned)
                    {
                        dg_sock::network_exception::throw_exception(dg_sock::network_exception::POISONED_CONTAINER);
                    }

                    if (!this->pop_argument_vec.empty())
                    {
                        *this->pop_argument_vec.front().updatable = std::move(updatable);
                        this->pop_argument_vec.front().smp->release();
                        this->pop_argument_vec.pop_front();

                        return false;
                    }

                    dg_sock::network_exception_handler::nothrow_log(this->push_argument_vec.push_back(PushArgument
                    {
                        .updatable  = std::move(updatable),
                        .smp        = &smp
                    }));

                    return true;
                }();

                if (need_wait)
                {
                    smp.acquire();                    
                }
            }

            auto pop() -> std::shared_ptr<UpdatableInterface>
            {
                std::binary_semaphore smp(0);
                std::shared_ptr<UpdatableInterface> rs{};

                bool need_wait = [&]
                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->is_poisoned)
                    {
                        dg_sock::network_exception::throw_exception(dg_sock::network_exception::POISONED_CONTAINER);
                    }

                    if (!this->push_argument_vec.empty())
                    {
                        rs = std::move(this->push_argument_vec.front().updatable);
                        this->push_argument_vec.front().smp->release();
                        this->push_argument_vec.pop_front();

                        return false;
                    }

                    dg_sock::network_exception_handler::nothrow_log(this->pop_argument_vec.push_back(PopArgument
                    {
                        .updatable  = &rs,
                        .smp        = &smp
                    }));

                    return true;
                }();

                if (need_wait)
                {
                    smp.acquire();
                }

                return rs;
            }

            void poison() noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                if (std::exchange(this->is_poisoned, true))
                {
                    return;
                }

                for (auto& e: this->push_argument_vec)
                {
                    e.smp->release();
                }

                for (auto& e: this->pop_argument_vec)
                {
                    e.smp->release();
                }

                this->push_argument_vec.clear();
                this->pop_argument_vec.clear();
            }
    };

    class CronWorker: public virtual dg_sock::network_concurrency::WorkerInterface
    {
        private:

            std::shared_ptr<CronContainerInterface> cron_container;

        public:

            CronWorker(std::shared_ptr<CronContainerInterface> cron_container)
            {
                if (cron_container == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->cron_container = std::move(cron_container);
            }

            auto run_one_epoch() noexcept -> bool
            {
                std::shared_ptr<UpdatableInterface> result;

                try
                {
                    result = this->cron_container->pop();
                }
                catch (...)
                {
                    dg_sock::network_log_stackdump::error_fast(dg_sock::network_exception::verbose(dg_sock::network_exception::wrap_std_exception(std::current_exception())));
                    return false;
                }

                if (result != nullptr)
                {
                    result->update();
                }

                return true;
            }
    };

    class CronLauncher
    {
        private:

            std::shared_ptr<CronContainerInterface> cron_container;
            std::shared_ptr<void> runner;

        public:

            CronLauncher()
            {
                this->cron_container    = std::make_shared<HandToHandCronContainer>();
                auto raw_runner         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_concurrency::daemon_saferegister(dg_sock::network_concurrency::UPDATE_DAEMON,
                                                                                                                                            std::make_unique<CronWorker>(this->cron_container)));
                this->runner            = std::make_shared<decltype(raw_runner)>(std::move(raw_runner));
            }

            ~CronLauncher() noexcept
            {
                this->cron_container->poison();
                this->runner = nullptr;
            }

            auto register_periodic_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                           std::chrono::nanoseconds dur) -> std::shared_ptr<void>
            {
                return cron_subsystem::register_periodic_cronjob(dg_sock::network_allocation::make_shared<UpdatableWrapper>(updatable, this->cron_container), dur);
            }

        private:

            class UpdatableWrapper: public virtual cron_subsystem::UpdatableInterface
            {
                private:

                    std::shared_ptr<dg_sock::network_cron::UpdatableInterface> base_job;
                    std::shared_ptr<CronContainerInterface> cron_container;

                public:

                    UpdatableWrapper(std::shared_ptr<dg_sock::network_cron::UpdatableInterface> base_job,
                                     std::shared_ptr<CronContainerInterface> cron_container): base_job(std::move(base_job)),
                                                                                              cron_container(std::move(cron_container)){}

                    void update()
                    {
                        this->cron_container->push(this->base_job);
                    }
            };
    };

    struct Signature{};

    using SingletonObject = stdxx::singleton<Signature, std::shared_ptr<CronLauncher>>;

    void init()
    {
        stdxx::memtransaction_guard tx_grd;
        SingletonObject::get() = std::make_shared<CronLauncher>();
    }

    void deinit()
    {
        stdxx::memtransaction_guard tx_grd;
        SingletonObject::get() = nullptr;
    }

    auto register_periodic_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                   std::chrono::nanoseconds dur) -> std::shared_ptr<void>
    {
        return SingletonObject::get()->register_periodic_cronjob(updatable, dur);
    }
}

#endif