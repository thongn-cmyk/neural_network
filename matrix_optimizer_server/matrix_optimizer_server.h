#ifndef __MATRIX_OPTIMIZER_SERVER_H__
#define __MATRIX_OPTIMIZER_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix_optimizer_subsystem/generic_matrix_optimizer.h>
#include <main_broker/main_service.h>
#include <concurrency_base/concurrency_base.h>
#include <request_extension/type_based_dgstd_resolutor.h>
#include <request_extension/type_based_resolutor_interface.h>

namespace matrix_optimizer_server
{
    static inline constexpr std::string_view MATRIX_OPTIMIZER_SERVER_VERSION_CONTROL = "";

    class ClientBox
    {
        private:

            struct Resource
            {
                std::optional<datasource::Configuration> datasource_config;
                std::optional<generic_matrix_factory::ExternalGenericMatrixResource> matrix_resource;
                std::optional<std::vector<dg_sock::network_rest_frame::model::Url>> url_vec;
                std::optional<optimizer::GenericOptimizerConfiguration> optimizer_config;
            };

            struct Deliverable
            {
                generic_matrix_factory::ExternalGenericMatrixResource matrix_resource;
                std::exception_ptr exception;
            };

            Resource resource;
            std::shared_ptr<std::thread> task_thr;
            bool was_run_broke;
            bool was_wait_broke;
            bool was_explicitly_destroyed;
            std::shared_ptr<std::atomic<bool>> is_completed_var;
            std::shared_ptr<std::atomic<bool>> interruption_pill;
            std::shared_ptr<Deliverable> deliverable;

        public:

            ClientBox(): resource(),
                         task_thr(),
                         was_run_broke(false),
                         was_wait_broke(false),
                         was_explicitly_destroyed(false),
                         is_completed_var(std::make_shared<std::atomic<bool>>(false)),
                         interruption_pill(std::make_shared<std::atomic<bool>>(false)),
                         deliverable(std::make_shared<Deliverable>(Deliverable{.matrix_resource = {}, .exception = nullptr})){}

            ~ClientBox() noexcept
            {
                this->close();
            }

            void set_data_source(const datasource::Configuration& config)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.datasource_config = config;
            }

            void set_matrix_resource(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.matrix_resource = matrix_resource;
            }

            void set_slave_endpoint_vector(const std::vector<dg_sock::network_rest_frame::model::Url>& url_vec)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.url_vec = url_vec;
            }

            void set_optimizer(const optimizer::GenericOptimizerConfiguration& config)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.optimizer_config = config;
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

                std::unique_ptr<InternalResolutor> resolutor = std::make_unique<InternalResolutor>(this->resource,
                                                                                                   this->is_completed_var,
                                                                                                   this->interruption_pill,
                                                                                                   this->deliverable);

                this->task_thr      = main_service::run(std::move(resolutor));
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
                this->interruption_pill->exchange(true, std::memory_order_relaxed);
            }

            auto wait() -> generic_matrix_factory::ExternalGenericMatrixResource
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

                this->task_thr->join();
                std::rethrow_exception(this->deliverable->exception);

                return this->deliverable->matrix_resource;
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

            class InternalResolutor: public virtual main_service::ThreadTaskInterface
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
                        if (!resource_arg.datasource_config.has_value())
                        {
                            throw std::invalid_argument("bad datasource config, not set");
                        }

                        if (!resource_arg.matrix_resource.has_value())
                        {
                            throw std::invalid_argument("bad matrix resource config, not set");
                        }

                        if (!resource_arg.url_vec.has_value())
                        {
                            throw std::invalid_argument("bad url vec, not set");
                        }

                        if (!resource_arg.optimizer_config.has_value())
                        {
                            throw std::invalid_argument("bad optimizer config, not set");
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
                        this->interruption_pill = std::move(interruption_pill));
                        this->deliverable       = std::move(deliverable);
                    }

                    void run(common_exception::CancellationTokenInterface& cancellation_token)
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
                    }

                private:

                    void internal_run(common_exception::CancellationTokenInterface& cancellation_token)
                    {
                        std::vector<std::unique_ptr<deviation_projection_client::ThreadSafe_APIClient_2>> client_vec{};
                        std::vector<uint64_t> projection_client_id_vec{};

                        for (const auto& url: this->resource.url_vec.value())
                        {
                            client_vec.push_back(std::make_unique<deviation_projection_client::ThreadSafe_APIClient_2>(url));
                            projection_client_id_vec.push_back(client_vec.back()->get_client_id());
                        }

                        common_exception::LambdaCancellationToken cancellation_token_obj([&]() noexcept
                        {
                            return cancellation_token.is_canceled() || this->interruption_pill->load(std::memory_order_relaxed);
                        });

                        data_ingestor::ClientTrainingDataIngestor{}.set_client_id_vec(projection_client_id_vec)
                                                                   .set_data_source(this->resource.datasource_config.value())
                                                                   .with_cancellation_token(cancellation_token_obj)
                                                                   .run();

                        std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator   = std::make_unique<matrix_evaluator::DistributedMatrixEvaluator>(projection_client_id_vec);

                        this->deliverable->matrix_resource = matrix_optimizer::DistributedOptimizer{}.set_matrix(this->resource.matrix_resource.value())
                                                                                                     .set_evaluator(*evaluator)
                                                                                                     .with_cancellation_token(cancellation_token_obj)
                                                                                                     .set_optimization_config(this->resource.optimizer_config.value())
                                                                                                     .optimize();
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

            void set_data_source(const datasource::Configuration& config)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_data_source(config);
            }

            void set_matrix_resource(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_matrix_resource(matrix_resource);
            }

            void set_slave_endpoint_vector(const std::vector<dg_sock::network_rest_frame::model::Url>& url_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_slave_endpoint_vector(url_vec);
            }

            void set_optimizer(const optimizer::GenericOptimizerConfiguration& config)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_optimizer(config);
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
                return this->base->is_completed();
            }

            void interrupt() noexcept
            {
                this->base->interrupt();
            }

            auto wait() -> generic_matrix_factory::ExternalGenericMatrixResource
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                return this->base->wait();
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

            void close_client_box(uint64_t client_box_id) noexcept
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
        std::expected<std::string, matrix_optimizer_server::local_exception_t> response;
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
        }
    };

    struct OpenClientResponse
    {
        std::expected<uint64_t, matrix_optimizer_server::local_exception_t> result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
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
        matrix_optimizer_server::local_exception_t err;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(err, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(err, err_verbal_description);
        }
    };

    struct RunRequest
    {
        uint64_t client_box_id;
        datasource::Configuration data_source_config;
        generic_matrix_factory::ExternalGenericMatrixResource matrix_resource;
        std::vector<dg_sock::network_rest_frame::model::Url> url_vec;
        optimizer::GenericOptimizerConfiguration optimizer_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id,
                      data_source_config,
                      matrix_resource,
                      url_vec,
                      optimizer_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id,
                      data_source_config,
                      matrix_resource,
                      url_vec,
                      optimizer_config);
        }
    };

    struct RunResponse
    {
        matrix_optimizer_server::local_exception_t result;
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
        matrix_optimizer_server::local_exception_t result;
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
        std::expected<bool, matrix_optimizer_server::local_exception_t> result;
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
        std::expected<generic_matrix_factory::ExternalGenericMatrixResource, matrix_optimizer_server::local_exception_t> result;
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

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/get_version";

            auto handle(const GetVersionRequest& request) -> GetVersionResponse
            {
                return GetVersionResponse
                {
                    .response = std::string(MATRIX_OPTIMIZER_SERVER_VERSION_CONTROL),
                    .err_verbal_description = {}
                };
            }
    };

    class OpenClientResolver: public virtual TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/open_client";

            OpenClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

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
                        .result = matrix_optimizer_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = matrix_optimizer_server::verbose_error_code(matrix_optimizer_server::to_local_exception_error_code(std::current_exception()))
                    }
                }
            }
    };

    class CloseClientResolver: public virtual TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/close_client";

            CloseClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const CloseClientRequest& request) -> CloseClientResponse
            {
                this->client_box_manager->close_client_box(request.client_box_id);

                return CloseClientResponse
                {
                    .result = matrix_optimizer_server::SUCCESS,
                    .err_verbal_description = ""
                };
            }
    };

    class RunResolver: public virtual TypeBasedResolutorInterface<RunRequest, RunResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/run";

            RunResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const RunRequest& request) -> RunResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    client_box->set_data_source(request.data_source_config);
                    client_box->set_matrix_resource(request.matrix_resource);
                    client_box->set_slave_endpoint_vector(request.url_vec);
                    client_box->set_optimizer(request.optimizer_config);
                    client_box->run();

                    return RunResponse
                    {
                        .result = matrix_optimizer_server::SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return RunResponse
                    {
                        .result = matrix_optimizer_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = matrix_optimizer_server::verbose_error_code(matrix_optimizer_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    class IsCompletedResolver: public virtual TypeBasedResolutorInterface<IsCompletedRequest, IsCompletedResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/is_completed";

            IsCompletedResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const IsCompletedRequest& request) -> IsCompletedResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    return IsCompletedResponse
                    {
                        .result = client_box->is_completed(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return IsCompletedResponse
                    {
                        .result = std::unexpected(matrix_optimizer_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = matrix_optimizer_server::verbose_error_code(matrix_optimizer_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    class GetResultResolver: public virtual TypeBasedResolutorInterface<GetResultRequest, GetResultResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/get_result";

            GetResultResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const GetResultRequest& request) -> GetResultResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    if (!client_box->is_completed())
                    {
                        throw optimization_in_progress_error{};
                    }

                    return GetResultResponse
                    {
                        .result = client_box->wait(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return GetResultResponse
                    {
                        .result = std::unexpected(matrix_optimizer_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = matrix_optimizer_server::verbose_error_code(matrix_optimizer_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    void init()
    {
        using namespace request_extension::resolutor;

        std::shared_ptr<ClientBoxManager> client_box_manager = std::make_shared<ClientBoxManager>();

        dg_sock::network_rest_frame::server_instance::hook(GetVersionResolver::RESOLVABLE_PATH, wrap(std::make_unique<GetVersionResolver>()));
        dg_sock::network_rest_frame::server_instance::hook(OpenClientResolver::RESOLVABLE_PATH, wrap(std::make_unique<OpenClientResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(CloseClientResolver::RESOLVABLE_PATH, wrap(std::make_unique<CloseClientResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(RunResolver::RESOLVABLE_PATH, wrap(std::make_unique<RunResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(IsCompletedResolver::RESOLVABLE_PATH, wrap(std::make_unique<IsCompletedResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(GetResultResolver::RESOLVABLE_PATH, wrap(std::make_unique<GetResultResolver>(client_box_manager)));
    }

    void deinit() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(GetResultResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(IsCompletedResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(RunResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(CloseClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(OpenClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(GetVersionResolver::RESOLVABLE_PATH);
    }
}

#endif