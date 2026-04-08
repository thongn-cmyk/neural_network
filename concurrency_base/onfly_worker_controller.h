#ifndef __ONFLY_WORKER_CONTROLLER_H__
#define __ONFLY_WORKER_CONTROLLER_H__

#include <stdint.h>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <common_exception/common_exception.h>
#include <mutex_extension/fair_mutex.h>
#include "concurrency_base_definition.h"
#include "worker_interface.h"
#include "rescheduler_interface.h"
#include "interruptable_worker_interface.h"
#include <main_service/main_service.h>
#include <main_service/thread_service.h>
#include <optional>

namespace concurrency_base::onfly_worker_controller
{
    using namespace concurrency_base::definition;
    using namespace concurrency_base::interface;

    struct DaemonInformation
    {
        size_t daemon_idx;
        daemon_kind_t daemon_kind;
    };

    struct DaemonControllerInterface
    {
        virtual ~DaemonControllerInterface() noexcept = default;

        virtual auto _register(daemon_kind_t daemon_kind, std::unique_ptr<InterruptableWorkerInterface>&& worker) -> size_t = 0;
        virtual void deregister(size_t registration_id) noexcept = 0; 

        virtual auto daemon_info(std::thread::id id) -> std::optional<DaemonInformation> = 0;
        virtual auto daemon_size(daemon_kind_t daemon_kind) -> size_t = 0;

        virtual auto daemon_set() -> std::vector<daemon_kind_t> = 0;
    };

    class FiniteDaemonController: public virtual DaemonControllerInterface
    {
        private:

            using resource_id_t = size_t;

            struct InformationBucket
            {
                std::vector<uint32_t> id_table;
                size_t cap;
            };

            struct ResourceBucket
            {
                std::shared_ptr<std::thread> daemon_runner;
                size_t daemon_idx;
                daemon_kind_t daemon_kind;
            };

            std::unordered_map<daemon_kind_t, InformationBucket> information_map;       
            std::unordered_map<std::thread::id, ResourceBucket> resource_map;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

            static inline constexpr std::chrono::nanoseconds YIELD_PERIOD = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1));

        public:

            FiniteDaemonController(const std::unordered_map<daemon_kind_t, size_t>& daemon_cap_map): information_map(),
                                                                                                     resource_map(),
                                                                                                     mtx(fair_mutex::make_unique_fair_atomic_flag())
            {
                for (const auto& [daemon_kind, daemon_cap]: daemon_cap_map)
                {
                    this->information_map[daemon_kind] = InformationBucket
                    {
                        .id_table   = this->get_iota_vec(daemon_cap),
                        .cap        = daemon_cap
                    };
                }
            }

            auto _register(daemon_kind_t daemon_kind, std::unique_ptr<InterruptableWorkerInterface>&& worker) -> resource_id_t
            {
                uint32_t tmp_id;

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (worker == nullptr)
                    {
                        common_exception::throw_exception(common_exception::INVALID_ARGUMENT);
                    }

                    auto map_ptr = this->information_map.find(daemon_kind);

                    if (map_ptr == this->information_map.end())
                    {
                        common_exception::throw_exception(common_exception::INVALID_ARGUMENT);
                    }

                    if (map_ptr->second.id_table.empty())
                    {
                        common_exception::throw_exception(common_exception::INVALID_ARGUMENT);
                    }

                    uint32_t tmp_id = map_ptr->second.id_table.back();
                    map_ptr->second.id_table.pop_back();
                }

                std::shared_ptr<std::thread> task;

                try
                {
                    task = this->get_thread_task(std::move(worker));
                }
                catch (...)
                {
                    {
                        fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                        this->information_map.at(daemon_kind).id_table.push_back(tmp_id);
                    }

                    throw;
                }

                try
                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    this->resource_map[task->get_id()] = ResourceBucket
                    {
                        .daemon_runner  = task,
                        .daemon_idx     = tmp_id,
                        .daemon_kind    = daemon_kind
                    };
                }
                catch (...)
                {
                    std::abort();
                }

                return std::bit_cast<resource_id_t>(task->get_id());
            }

            void deregister(resource_id_t resource_id) noexcept
            {
                std::shared_ptr<std::thread> disposable;

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    std::thread::id thr_id = std::bit_cast<std::thread::id>(resource_id);
                    size_t daemon_idx;
                    daemon_kind_t daemon_kind;

                    {
                        auto map_ptr = this->resource_map.find(thr_id);

                        if (map_ptr == this->resource_map.end())
                        {
                            std::abort();
                        }

                        disposable  = map_ptr->second.daemon_runner;
                        daemon_idx  = map_ptr->second.daemon_idx;
                        daemon_kind = map_ptr->second.daemon_kind;

                        this->resource_map.erase(map_ptr);
                    }

                    {
                        auto map_ptr = this->information_map.find(daemon_kind);

                        if (map_ptr == this->information_map.end())
                        {
                            std::abort();
                        }

                        map_ptr->second.id_table.push_back(daemon_idx);
                    }
                }
            }

            auto daemon_info(std::thread::id id) -> std::optional<DaemonInformation>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                auto map_ptr = this->resource_map.find(id);

                if (map_ptr == this->resource_map.end())
                {
                    return std::nullopt;
                }

                return DaemonInformation
                {
                    .daemon_idx     = map_ptr->second.daemon_idx,
                    .daemon_kind    = map_ptr->second.daemon_kind           
                };
            }

            auto daemon_size(daemon_kind_t daemon_kind) -> size_t
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                auto map_ptr = this->information_map.find(daemon_kind);

                if (map_ptr == this->information_map.end())
                {
                    return 0u;
                }

                return map_ptr->second.cap;
            }

            auto daemon_set() -> std::vector<daemon_kind_t>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                auto rs = std::vector<daemon_kind_t>{};

                for (const auto& [daemon_kind, _]: this->information_map)
                {
                    rs.push_back(daemon_kind);
                }

                return rs;
            }

        private:

            static auto get_iota_vec(size_t sz) -> std::vector<uint32_t>
            {
                if (sz > std::numeric_limits<uint32_t>::max())
                {
                    common_exception::throw_exception(common_exception::INVALID_ARGUMENT);
                }

                std::vector<uint32_t> rs(sz);
                std::iota(rs.begin(), rs.end(), 0u);

                return rs;
            }

            class ThreadRunnableWrapper: public virtual main_service::thread_service::TaskInterface
            {
                private:

                    std::unique_ptr<InterruptableWorkerInterface> worker;

                    static inline constexpr size_t CHK_INTERVAL_SZ = size_t{1} << 3;

                public:
 
                    ThreadRunnableWrapper(std::unique_ptr<InterruptableWorkerInterface>&& worker)
                    {
                        if (worker == nullptr)
                        {
                            common_exception::throw_exception(common_exception::INVALID_ARGUMENT);
                        }

                        this->worker = std::move(worker);
                    }

                    void run(common_exception::CancellationTokenInterface& cancellation_token) noexcept
                    {
                        while (true)
                        {
                            if (cancellation_token.is_canceled())
                            {
                                break;
                            }

                            try
                            {
                                for (size_t i = 0u; i < CHK_INTERVAL_SZ; ++i)
                                {
                                    if (!this->worker->run_one_epoch(cancellation_token))
                                    {
                                        std::this_thread::sleep_for(YIELD_PERIOD);
                                        break;
                                    }
                                }
                            }
                            catch (common_exception::operation_graceful_termination_error& e)
                            {
                                break;
                            }
                            catch (...)
                            {
                                logging_subsystem::log(logging_subsystem::LogFactory{}.topic("concurrency_base::onfly_worker_controller")
                                                                                      .topic("thread_runner")
                                                                                      .message(std::current_exception()));

                                break;
                            }
                        }
                    }

                    auto get() -> std::unique_ptr<InterruptableWorkerInterface>&
                    {
                        return this->worker;
                    }
            };

            auto get_thread_task(std::unique_ptr<InterruptableWorkerInterface>&& worker) -> std::shared_ptr<std::thread>
            {
                std::shared_ptr<main_service::thread_service::TaskInterface> task = std::make_shared<ThreadRunnableWrapper>(std::move(worker));

                try
                {
                    return main_service::broke_thread(task);   
                }
                catch (...)
                {
                    worker = std::move(std::dynamic_pointer_cast<ThreadRunnableWrapper>(task)->get());
                    throw;
                }
            }
    };
}

#endif