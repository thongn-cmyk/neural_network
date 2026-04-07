#ifndef __DEVIATION_PROJECTION_INGESTION_AID_SERVER_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <main_broker/main_service.h>
#include <concurrency_base/concurrency_base.h>
#include <request_extension/type_based_resolutor_interface.h>
#include <request_extension/type_based_dgstd_resolutor.h>
#include <deviation_projection_client/deviation_projection_client.h>
#include <fire_bandwidth_control/temporal_firer.h>
#include <data_loader/source_loader/generic_loader.h>
#include <memory>
#include <atomic>

namespace deviation_projection_ingestion_aid_server
{
    using local_exception_t = uint8_t;

    //today, we'd have to have thread_local variable to store certain concurrency data, in conjunction with on-fly thread instances by leveraging main_service
    //today, we'd work on the construction of unbreakable encoding method for security
    //it's gonna be a tough day

    struct ServerSink
    {
        dg_sock::network_rest_frame::model::Remote remote;
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(remote, client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(remote, client_id);
        }
    };

    class ClientBox
    {
        private:

            struct Resource
            {
                std::optional<data_loader::source_loader::generic_loader::GenericLoaderConfig> data_loader_config;
                std::optional<std::vector<ServerSink>> server_sink_vec;
                std::optional<fire_bandwidth_control::generic_firer::GenericFirerConfig> token_firer_config;
                std::optional<dg_sock::network_rest_frame::client::retry_policy_t> client_retry_policy;
                std::optional<size_t> concurrent_request_sz;
            };

            struct Deliverable
            {
                std::exception_ptr exception;
            };

            Resource resource;
            std::shared_ptr<void> task_thr;
            bool was_run_broke;
            bool was_wait_broke;
            bool was_explicitly_destroyed;
            std::shared_ptr<std::atomic<bool>> is_completed_var;
            std::shared_ptr<std::atomic<bool>> interruption_pill;
            std::shared_ptr<Deliverable> deliverable;

        public:

            ClientBox(): resource(),
                         task_thr(nullptr),
                         was_run_broke(false),
                         was_wait_broke(false),
                         was_explicitly_destroyed(false),
                         is_completed_var(std::make_shared<std::atomic<bool>>(false)),
                         interruption_pill(std::make_shared<std::atomic<bool>>(false)),
                         deliverable(std::make_shared<Deliverable>(Deliverable{.exception = nullptr})){}

            void set_data_source(const data_loader::source_loader::generic_loader::GenericLoaderConfig& data_loader_config)
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
                    throw std::runtime_error("second run");
                }

                if (this->interruption_pill->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("interrupted run");
                }

                std::unique_ptr<InternalResolutor> resolutor = std::make_unique<InternalResolutor>(this->resource,
                                                                                                   this->is_completed_var,
                                                                                                   this->interruption_pill,
                                                                                                   this->deliverable);

                auto tmp            = concurrency_base::daemon_saferegister(concurrency_base::COMMON_ONFLY_POOL, std::move(resolutor));

                if (!tmp.has_value())
                {
                    common_exception::throw_exception(tmp.error());
                }

                this->task_thr      = std::move(tmp.value());
                this->was_run_broke = true;
            }

            auto is_completed() -> bool
            {
                if (this->was_explicitly_destroyed)
                {
                    return true;
                }

                return this->is_completed_var->load(std::memory_order_relaxed);
            }

            void interrupt() noexcept
            {
                if (this->was_explicitly_destroyed)
                {
                    return;
                }

                this->interruption_pill->exchange(true, std::memory_order_relaxed);
            }

            void wait()
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                if (!this->was_run_broke)
                {
                    throw wait_before_run_error{};
                }

                if (std::exchange(this->was_wait_broke, true))
                {
                    throw second_wait_error{};
                }

                this->task_thr = nullptr;
                this->is_compelted_var->wait(false, std::memory_order_acquire);

                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }

                std::rethrow_exception(this->deliverable->exception);
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
                    this->task_thr = nullptr;
                }
            }

        private:

            class FirehoseProducer: public virtual fire_bandwidth_control::interface::FireableInterface
            {
                private:

                    data_loader::source_loader::UserSpaceSourceLoaderInterface * source_loader;
                    std::vector<std::unique_ptr<deviation_projection_client::NoOwned_APIClient>> * api_client_vec;

                    std::deque<std::unique_ptr<dg_sock::network_rest_frame::client::Promise<stdx::fancy_void>>> promise_vec;
                    bool is_loader_completed;
                    size_t client_offset;
                    size_t concurrent_request_sz;

                    static inline constexpr size_t MIN_CONCURRENT_REQUEST_SZ    = 1u;
                    static inline constexpr size_t MAX_CONCURRENT_REQUEST_SZ    = size_t{1} << 10;

                public:

                    FirehoseProducer(data_loader::source_loader::UserSpaceSourceLoaderInterface * source_loader,
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

                        if (!nxt_token.has_next())
                        {
                            this->is_loader_completed = true;
                            return true;
                        }

                        this->promise_vec.push_back(this->api_client_vec->operator[](this->client_offset % this->api_client_vec.size()).add_training_data_2(nxt_token.value()));
                        this->client_offset++;

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

                    std::atomic<bool> * interruption_pill;
                    common_exception::CancellationTokenInterface * thr_cancellation_token;
                    bool is_canceled_once;

                public:

                    static inline constexpr size_t DICE_CHANCE = size_t{1} << 6;

                    InternalCancellationToken(std::atomic<bool> * interruption_pill,
                                              common_exception::CancellationTokenInterface * thr_cancellation_token): interruption_pill(stdx::safe_ptr_access(interruption_pill)),
                                                                                                                      thr_cancellation_token(stdx::safe_ptr_access(thr_cancellation_token)),
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

                            if (this->interruption_pill->load(std::memory_order_relaxed))
                            {
                                this->is_canceled_once = true;
                                return true;
                            }
                        }

                        return false;
                    }
            };

            class InternalResolutor: public virtual concurrency_base::InterruptableWorkerInterface
            {
                private:

                    Resource resource;
                    std::shared_ptr<std::atomic<bool>> is_completed_var;
                    std::shared_ptr<std::atomic<bool>> interruption_pill;
                    std::shared_ptr<Deliverable> deliverable;

                public:

                    InternalResolutor(Resource resource_arg,
                                      std::shared_ptr<std::atomic<bool>> is_completed_var,
                                      std::shared_ptr<std::atomic<bool>> interruption_pill,
                                      std::shared_ptr<Deliverable> deliverable)
                    {
                        if (!resource_arg.data_loader_config.has_value())
                        {
                            throw std::invalid_argument("bad data loader config, not set");
                        }

                        if (!resource_arg.server_sink_vec.has_value())
                        {
                            throw std::invalid_argument("bad server sink(s), not set");
                        }

                        if (!resource_arg.data_firehose_config.has_value())
                        {
                            throw std::invalid_argument("bad data firehose config, not set"); //default
                        }

                        if (!resource_arg.client_retry_policy.has_value())
                        {
                            throw std::invalid_argument("bad client retry policy, not set"); //default
                        }

                        if (!resource_arg.concurrent_request_sz.has_value())
                        {
                            throw std::invalid_argument("bad concurrent request size, not set"); //default
                        }

                        if (is_completed_var == nullptr)
                        {
                            throw std::invalid_argument("bad is completed var, null");
                        }

                        if (interruption_pill == nullptr)
                        {
                            throw std::invalid_argument("bad interruption pill, null");
                        }

                        if (deliverable == nullptr)
                        {
                            throw std::invalid_argument("bad deliverable, null");
                        }

                        this->resource          = std::move(resource_arg);
                        this->is_completed_var  = std::move(is_completed_var);
                        this->interruption_pill = std::move(interruption_pill);
                        this->deliverable       = std::move(deliverable);
                    }

                    auto run_one_epoch(common_exception::CancellationTokenInterface& cancellation_token) -> bool
                    {
                        try
                        {
                            this->internal_run(cancellation_token);

                            this->deliverable->exception = nullptr;
                        }
                        catch (...)
                        {
                            this->deliverable->exception = std::current_exception();
                        }

                        this->is_completed_var->exchange(true, std::memory_order_release);

                        if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                        {
                            std::atomic_thread_fence(std::memory_order_seq_cst);
                        }

                        return false;
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

                        InternalCancellationToken arg_cancellation_token(this->interruption_pill.get(), &cancellation_token);

                        std::unique_ptr<data_loader::source_loader::UserSpaceSourceLoaderInterface> loader  = std::make_unique<data_loader::source_loader::GenericLoader>(this->resource.data_loader_config.value());
                        std::unique_ptr<fire_bandwidth_control::FireableFirerInterface> firer_instance      = std::make_unique<fire_bandwidth_control::GenericFirer>(this->resource.token_firer_config.value());

                        FirehoseProducer firehose_producer(loader.get(),
                                                           &api_client_vec,
                                                           this->resource.concurrent_request_sz.value());

                        firer_instance->run(firehose_producer, arg_cancellation_token);
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

            void set_data_source(const data_loader::Configuration& config)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->add_data_source(config);
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

                if (this->was_explicitly_destroyed.exchange(true, std::memory_order_relaxed))
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

    struct GetVersionRequest
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    struct GetVersionResponse
    {
        std::expected<std::string, deviation_projection_ingestion_aid_server::local_exception_t> response;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(response, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(response, err_verbal_description);
        }
    };

    struct OpenClientRequest
    {
        connectivity_subsystem::SlaveConfiguration connection_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(connection_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(connection_config);
        };
    };

    struct OpenClientResponse
    {
        std::expected<uint64_t, deviation_projection_ingestion_aid_server::local_exception_t> result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct CloseClientRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct CloseClientResponse
    {
        deviation_projection_ingestion_aid_server::local_exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct RunRequest
    {
        uint64_t client_box_id;
        std::vector<std::pair<uint64_t, dg_sock::network_rest_frame::model::Url>> server_box_vec;
        data_loader::Configuration config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id, server_box_vec, config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id, server_box_vec, config);
        }
    };

    struct RunResponse
    {
        deviation_projection_ingestion_aid_server::local_exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct InterruptRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct InterruptResponse
    {
        deviation_projection_ingestion_aid_server::local_exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct IsCompletedRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct IsCompletedResponse
    {
        std::expected<bool, deviation_projection_ingestion_aid_server::local_exception_t> result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct GetResultRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct GetResultResponse
    {
        deviation_projection_ingestion_aid_server::local_exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    template <class T_In, class T_Out>
    using TypeBasedResolutorInterface = request_extension::resolutor::TypeBasedResolutorInterface<T_In, T_Out>;

    class GetVersionResolver: public virtual TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>
    {
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/get_version";

            auto handle(const GetVersionRequest& request) -> GetVersionResponse
            {
                (void) request;

                return GetVersionResponse
                {
                    .response = std::string(DEVIATION_PROJECTION_INGESTION_AID_SERVER_VERSION_CONTROL),
                    .err_verbal_description = ""
                };
            }
    };

    class OpenClientResolver: public virtual TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/open_client";

            OpenClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager): client_box_manager(std::move(client_box_manager)){}

            auto handle(const OpenClientRequest& request) -> OpenClientResponse
            {
                try
                {
                    uint64_t client_box_id = this->client_box_manager->open_client_box(request.connection_config);

                    return OpenClientResponse
                    {
                        .result = client_box_id,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return OpenClientResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_error_code(deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };
    
    class CloseClientResolver: public virtual TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/close_client";

            CloseClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const CloseClientRequest& request) -> CloseClientResponse
            {   
                this->client_box_manager->close_client_box(request.client_box_id);

                return CloseClientResponse
                {
                    .result = SUCCESS,
                    .err_verbal_description = ""
                };
            }
    };

    class RunResolver: public virtual TypeBasedResolutorInterface<RunRequest, RunResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/run";

            RunResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const RunRequest& request) -> RunResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    client->set_data_source(request.config);
                    client->set_server_sink(request.server_box_vec);
                    client->run();

                    return RunResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return RunResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_error_code(deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    class InterruptResolver: public virtual TypeBasedResolutorInterface<InterruptRequest, InterruptResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/interrupt";

            InterruptResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const InterruptRequest& request) -> InterruptResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    client->interrupt();

                    return InterruptResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return InterruptResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_error_code(deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    class IsCompletedResolver: public virtual TypeBasedResolutorInterface<IsCompletedRequest, IsCompletedResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/is_completed";

            IsCompletedResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const IsCompletedRequest& request) -> IsCompletedResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    return IsCompletedResponse
                    {
                        .result = client->is_completed(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return IsCompletedResponse
                    {
                        .result = std::unexpected(deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_error_code(deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    class GetResultResolver: public virtual TypeBasedResolutorInterface<GetResultRequest, GetResultResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/get_result";

            GetResultResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const GetResultRequest& request) -> GetResultResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    client->wait();

                    return GetResultResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return GetResultResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_error_code(deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };
}

#endif