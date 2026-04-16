#ifndef __CONNECTION_BASED_MANAGER_H__
#define __CONNECTION_BASED_MANAGER_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex_extension/fair_mutex.h>
#include <cron_subsystem/cron_subsystem.h>

namespace connection_based_manager
{
    //thread-safe object
    class HealthcheckableInterface
    {
        public:
            
            virtual ~HealthcheckableInterface() noexcept = default;

            virtual auto is_alive() -> bool = 0;
    };

    class ManagerInterface
    {
        public:

            virtual ~ManagerInterface() noexcept = default;

            virtual auto add(const std::shared_ptr<HealthcheckableInterface>& healthcheckable) -> uint64_t = 0;
            virtual auto get(uint64_t id) -> std::shared_ptr<HealthcheckableInterface> = 0;
            virtual void close(uint64_t id) noexcept = 0;
    };

    class ClientManagerBase: public virtual ManagerInterface,
                             public virtual cron_subsystem::UpdatableInterface
    {
        private:

            std::unordered_set<uintptr_t> registered_healthcheckable_set;
            std::unordered_map<uint64_t, std::shared_ptr<HealthcheckableInterface>> client_map;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            uint64_t id_counter;

        public:

            ClientManagerBase(): registered_healthcheckable_set(),
                                 client_map(),
                                 mtx(fair_mutex::make_unique_fair_atomic_flag()),
                                 id_counter(0u){}

            auto add(const std::shared_ptr<HealthcheckableInterface>& healthcheckable) -> uint64_t
            {
                if (healthcheckable == nullptr)
                {
                    throw std::invalid_argument("bad healthcheckable, null");
                }

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->registered_healthcheckable_set.contains(reinterpret_cast<uintptr_t>(healthcheckable.get())))
                {
                    throw std::runtime_error("second healthcheckable");
                }

                try
                {
                    this->registered_healthcheckable_set.insert(reinterpret_cast<uintptr_t>(healthcheckable.get()));

                    uint64_t nxt_id             = this->id_counter;
                    this->client_map[nxt_id]    = healthcheckable;
                    this->id_counter            = nxt_id + 1u;

                    return nxt_id;
                }
                catch (...)
                {
                    std::abort();
                }
            }

            auto get(uint64_t id) -> std::shared_ptr<HealthcheckableInterface>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (auto map_ptr = this->client_map.find(id); map_ptr != this->client_map.end())
                {
                    return map_ptr->second;
                }

                return nullptr;
            }

            void close(uint64_t id) noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (auto map_ptr = this->client_map.find(id); map_ptr != this->client_map.end())
                {
                    this->registered_healthcheckable_set.erase(reinterpret_cast<uintptr_t>(map_ptr->second.get()));
                }

                this->client_map.erase(id);
            }

            void update()
            {
                std::vector<std::pair<uint64_t, std::shared_ptr<HealthcheckableInterface>>> client_vec{};

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                    std::copy(this->client_map.begin(), this->client_map.end(), std::back_inserter(client_vec));
                }

                std::unordered_set<uint64_t> bad_client_id_set{};

                for (const auto& [client_id, client_instance]: client_vec)
                {
                    bool is_bad_client;

                    try
                    {
                        is_bad_client = !client_instance->is_alive();
                    }
                    catch (...)
                    {
                        is_bad_client = true;
                    }

                    if (is_bad_client)
                    {
                        bad_client_id_set.insert(client_id);
                    }
                }

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    decltype(this->client_map) new_client_map{};
                    decltype(this->registered_healthcheckable_set) new_registered_healthcheckable_set{};

                    for (const auto& [client_id, client_instance]: this->client_map)
                    {
                        if (!bad_client_id_set.contains(client_id))
                        {
                            new_client_map.insert({client_id, client_instance});
                            new_registered_healthcheckable_set.insert(reinterpret_cast<uintptr_t>(client_instance.get()));
                        }
                    }

                    this->client_map = std::move(new_client_map);
                    this->registered_healthcheckable_set = std::move(new_registered_healthcheckable_set);
                }
            }
    };

    class ClientManager: public virtual ManagerInterface
    {
        private:

            std::shared_ptr<ClientManagerBase> base;
            std::shared_ptr<void> cron_obj;

        public:

            static inline const std::chrono::nanoseconds CRON_DURATION = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));

            ClientManager()
            {
                this->base      = std::make_shared<ClientManagerBase>();
                this->cron_obj  = cron_subsystem::register_periodic_cronjob(this->base, CRON_DURATION);
            }

            auto add(const std::shared_ptr<HealthcheckableInterface>& healthcheckable) -> uint64_t
            {
                return this->base->add(healthcheckable);
            }

            auto get(uint64_t id) -> std::shared_ptr<HealthcheckableInterface>
            {
                return this->base->get(id);
            }

            void close(uint64_t id) noexcept
            {
                this->base->close(id);
            }
    };
}

#endif