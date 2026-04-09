#ifndef __DEVIATION_PROJECTION_INGESTION_AID_SERVER_CLIENT_BOX_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_SERVER_CLIENT_BOX_H__

#include <stdint.h>
#include <stdlib.h>
#include <concurrency_base/concurrency_base.h>
#include <request_extension/type_based_resolutor_interface.h>
#include <request_extension/type_based_dgstd_resolutor.h>
#include <deviation_projection_client/deviation_projection_client.h>
// #include <fire_bandwidth_control/generic_firer.h>
#include <data_loader/source_loader/multisource_loader.h>
#include <memory>
#include <atomic>
#include "local_exception.h"
#include "model.h"
#include <common_exception/common_exception.h>
#include <connection_based_manager/connection_based_manager.h>
#include <mutex_extension/fair_mutex.h>
#include <exception>
#include <affined_randomizer/affined_randomizer.h>
#include <logging_subsystem/logging_subsystem.h>

#include <concurrency_detachable_task/detachable_launcher.h>

namespace deviation_projection_ingestion_aid_server
{
    class ClientBox
    {
        private:

            struct Resource
            {
                std::optional<data_loader::source_loader::multisource_loader::MultisourceLoaderConfig> data_loader_config;
                std::optional<std::vector<ServerSink>> server_sink_vec;
                std::optional<fire_bandwidth_control::generic_firer::GenericFirerConfig> token_firer_config;
                std::optional<dg_sock::network_rest_frame::client::retry_policy_t> client_retry_policy;
                std::optional<size_t> concurrent_request_sz;
            };

            Resource resource;
            std::shared_ptr<concurrency_detachable_task::DetachableTaskHandleInterface<bool>> task_handle;
            bool was_run_broke;
            bool was_wait_broke;
            bool was_explicitly_destroyed;

        public:

            ClientBox(): resource(),
                         task_handle(nullptr),
                         was_run_broke(false),
                         was_wait_broke(false),
                         was_explicitly_destroyed(false){}

            ~ClientBox() noexcept
            {
                this->close();
            }

            void set_data_source(const data_loader::source_loader::multisource_loader::MultisourceLoaderConfig& data_loader_config)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.data_loader_config = data_loader_config;
            }

            void set_server_sink(const std::vector<ServerSink>& server_sink_vec)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.server_sink_vec = server_sink_vec;
            }

            void set_firer_config(const fire_bandwidth_control::generic_firer::GenericFirerConfig& token_firer_config)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.token_firer_config = token_firer_config;
            }

            void set_concurrent_request_size(size_t sz)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.concurrent_request_sz = sz;
            }

            void set_client_retry_policy(const dg_sock::network_rest_frame::client::retry_policy_t& client_retry_policy)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.client_retry_policy = client_retry_policy;
            }

            void run()
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                if (this->was_run_broke)
                {
                    throw second_run_error{};
                }

                std::shared_ptr<concurrency_task::TaskInterface<bool>> resolutor = std::make_unique<InternalResolutor>(this->resource);

                this->task_handle   = concurrency_detachable_task::DetachableTaskLauncher{}.launch(std::move(resolutor));
                this->was_run_broke = true;
            }

            auto is_completed() -> bool
            {
                if (!this->was_run_broke)
                {
                    throw run_not_invoked_error{};
                }

                if (this->was_explicitly_destroyed)
                {
                    return true;
                }

                if (this->task_handle == nullptr)
                {
                    std::abort();
                }

                return this->task_handle->is_completed();
            }

            void interrupt() noexcept
            {
                if (this->was_explicitly_destroyed)
                {
                    return;
                }

                if (this->task_handle == nullptr)
                {
                    return;
                }

                this->task_handle->interrupt();
            }

            void wait()
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                if (!this->was_run_broke)
                {
                    throw run_not_invoked_error{};
                }

                if (std::exchange(this->was_wait_broke, true))
                {
                    throw second_wait_error{};
                }

                if (this->task_handle == nullptr)
                {
                    std::abort();
                }

                bool rs = this->task_handle->wait();

                if (!rs)
                {
                    std::abort();
                }
            }

            void close() noexcept
            {
                if (std::exchange(this->was_explicitly_destroyed, true))
                {
                    return;
                }

                if (!this->was_run_broke)
                {
                    return;
                }

                if (!this->was_wait_broke)
                {
                    if (this->task_handle == nullptr)
                    {
                        std::abort();
                    }

                    this->task_handle->detach();
                }
            }

        private:

            class FireableProducer: public virtual fire_bandwidth_control::interface::FireableInterface
            {
                private:

                    data_loader::source_loader::UserSpaceSourceLoaderInterface * source_loader;
                    std::vector<std::unique_ptr<deviation_projection_client::NoOwned_APIClient>> * api_client_vec;

                    std::deque<std::shared_ptr<dg_sock::network_rest_frame::client::Promise<stdx::fancy_void>>> promise_vec;
                    bool is_loader_completed;
                    size_t client_offset;
                    size_t concurrent_request_sz;

                    static inline constexpr size_t MIN_CONCURRENT_REQUEST_SZ    = 1u;
                    static inline constexpr size_t MAX_CONCURRENT_REQUEST_SZ    = size_t{1} << 10;

                public:

                    FireableProducer(data_loader::source_loader::UserSpaceSourceLoaderInterface * source_loader,
                                     std::vector<std::unique_ptr<deviation_projection_client::NoOwned_APIClient>> * api_client_vec,
                                     size_t concurrent_request_sz): source_loader(stdx::safe_ptr_access(source_loader)),
                                                                    api_client_vec(stdx::safe_ptr_access(api_client_vec)),
                                                                    promise_vec(),
                                                                    is_loader_completed(false),
                                                                    client_offset(0u),
                                                                    concurrent_request_sz(std::clamp(concurrent_request_sz, MIN_CONCURRENT_REQUEST_SZ, MAX_CONCURRENT_REQUEST_SZ)){}

                    auto fire_one(common_exception::CancellationTokenInterface& cancellation_token) -> bool
                    {
                        if (cancellation_token.is_canceled())
                        {
                            common_exception::throw_exception(common_exception::OPERATION_CANCELED_ERROR);
                        }

                        if (this->is_loader_completed)
                        {
                            this->wait_all_promise_vec();
                            return false;
                        }

                        if (this->promise_vec.size() == this->concurrent_request_sz)
                        {
                            this->promise_vec.front()->wait();
                            this->promise_vec.pop_front();
                        }

                        std::optional<std::string> nxt_token = this->source_loader->get(cancellation_token);

                        if (!nxt_token.has_value())
                        {
                            this->is_loader_completed = true;
                            return true;
                        }

                        size_t slot = this->client_offset % this->api_client_vec->size();
                        this->promise_vec.push_back((*this->api_client_vec)[slot]->add_training_data(nxt_token.value()));
                        this->client_offset += 1;

                        return true;
                    }
                
                private:

                    void wait_all_promise_vec()
                    {
                        for (const auto& promise: this->promise_vec)
                        {
                            promise->wait();
                        }

                        this->promise_vec.clear();
                    }
            };

            class InternalCancellationToken: public virtual common_exception::CancellationTokenInterface
            {
                private:

                    common_exception::CancellationTokenInterface * thr_cancellation_token;
                    bool is_canceled_once;

                public:

                    static inline constexpr size_t DICE_CHANCE = size_t{1} << 6;

                    InternalCancellationToken(common_exception::CancellationTokenInterface * thr_cancellation_token): thr_cancellation_token(stdx::safe_ptr_access(thr_cancellation_token)),
                                                                                                                      is_canceled_once(false){}

                    auto is_canceled() noexcept -> bool
                    {
                        if (this->is_canceled_once)
                        {
                            return true;
                        }

                        if (affined_randomizer::randomize_int<uint8_t>() % DICE_CHANCE == 0u)
                        {
                            if (this->thr_cancellation_token->is_canceled())
                            {
                                this->is_canceled_once = true;
                                return true;
                            }
                        }

                        return false;
                    }
            };

            class InternalResolutor: public virtual concurrency_task::TaskInterface<bool>
            {
                private:

                    Resource resource;

                public:

                    InternalResolutor(Resource resource_arg)
                    {
                        if (!resource_arg.data_loader_config.has_value())
                        {
                            throw std::invalid_argument("bad data loader config, not set");
                        }

                        if (!resource_arg.server_sink_vec.has_value())
                        {
                            throw std::invalid_argument("bad server sink(s), not set");
                        }

                        if (!resource_arg.token_firer_config.has_value())
                        {
                            throw std::invalid_argument("bad token firer config, not set"); //default
                        }

                        if (!resource_arg.client_retry_policy.has_value())
                        {
                            throw std::invalid_argument("bad client retry policy, not set"); //default
                        }

                        if (!resource_arg.concurrent_request_sz.has_value())
                        {
                            throw std::invalid_argument("bad concurrent request size, not set"); //default
                        }

                        this->resource  = std::move(resource_arg);
                    }

                    auto run(common_exception::CancellationTokenInterface& cancellation_token) -> bool
                    {
                        this->internal_run(cancellation_token);
                        return true;
                    }

                private:

                    void internal_run(common_exception::CancellationTokenInterface& cancellation_token)
                    {
                        std::vector<std::unique_ptr<deviation_projection_client::NoOwned_APIClient>> api_client_vec{};

                        for (const ServerSink& sink: resource.server_sink_vec.value())
                        {
                            api_client_vec.push_back(std::make_unique<deviation_projection_client::NoOwned_APIClient>(sink.remote, sink.client_id));
                            api_client_vec.back()->set_retry_policy(this->resource.client_retry_policy.value());
                        }

                        InternalCancellationToken arg_cancellation_token(&cancellation_token);

                        std::unique_ptr<data_loader::source_loader::UserSpaceSourceLoaderInterface> loader          = std::make_unique<data_loader::source_loader::multisource_loader::MultisourceLoader>(this->resource.data_loader_config.value());
                        std::unique_ptr<fire_bandwidth_control::interface::FireableFirerInterface> firer_instance   = std::make_unique<fire_bandwidth_control::generic_firer::GenericFirer>(this->resource.token_firer_config.value());

                        FireableProducer fireable_producer(loader.get(),
                                                           &api_client_vec,
                                                           this->resource.concurrent_request_sz.value());

                        firer_instance->run(fireable_producer, arg_cancellation_token);
                    }
            };
    };

    class ConnectionBoundClientBox: public virtual connection_based_manager::HealthcheckableInterface
    {
        private:

            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            std::unique_ptr<ClientBox> base;
            std::unique_ptr<std::atomic<bool>> was_explicitly_destroyed;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ConnectionBoundClientBox(const connectivity_subsystem::SlaveConfiguration& connection_config): connection(std::make_unique<connectivity_subsystem::ThreadSafeSlaveConnection>(connection_config)),
                                                                                                           base(std::make_unique<ClientBox>()),
                                                                                                           was_explicitly_destroyed(std::make_unique<std::atomic<bool>>(false)),
                                                                                                           mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void set_data_source(const data_loader::source_loader::multisource_loader::MultisourceLoaderConfig& config)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_data_source(config);
            }

            void set_server_sink(const std::vector<ServerSink>& server_sink_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_server_sink(server_sink_vec);
            }

            void set_firer_config(const fire_bandwidth_control::generic_firer::GenericFirerConfig& token_firer_config)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_firer_config(token_firer_config);
            }

            void set_concurrent_request_size(size_t sz)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_concurrent_request_size(sz);
            }

            void set_client_retry_policy(const dg_sock::network_rest_frame::client::retry_policy_t& client_retry_policy)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_client_retry_policy(client_retry_policy);
            }

            void run()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->run();
            }

            auto is_completed() -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->was_explicitly_destroyed->load(std::memory_order_relaxed) || this->base->is_completed();
            }

            void interrupt() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    return;
                }

                this->base->interrupt();
            }

            void wait()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->wait();
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->exchange(true, std::memory_order_relaxed))
                {
                    return;
                }

                this->connection->close();
                this->base->close();
            }

            auto is_alive() -> bool
            {
                return !this->was_explicitly_destroyed->load(std::memory_order_relaxed) && this->connection->is_alive();
            }
    };

    class ClientBoxManager
    {
        private:

            std::unique_ptr<connection_based_manager::ManagerInterface> base;

        public:

            ClientBoxManager(): base(std::make_unique<connection_based_manager::ClientManager>()){}

            auto open_client_box(const connectivity_subsystem::SlaveConfiguration& connection_config) -> uint64_t
            {
                return this->base->add(std::make_shared<ConnectionBoundClientBox>(connection_config));
            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {
                return std::dynamic_pointer_cast<ConnectionBoundClientBox>(this->base->get(client_box_id));
            }

            void close_client_box(uint64_t client_box_id)
            {
                this->base->close(client_box_id);
            }
    };
}

#endif