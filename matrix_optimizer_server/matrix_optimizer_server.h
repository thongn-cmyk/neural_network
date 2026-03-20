#ifndef __MATRIX_OPTIMIZER_SERVER_H__
#define __MATRIX_OPTIMIZER_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix_optimizer_subsystem/generic_matrix_optimizer.h>
#include <main_broker/main_service.h>
#include <concurrency_base/concurrency_base.h>

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

            ~ClientBox()
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

            auto is_completed() noexcept -> bool
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

            void wait() -> generic_matrix_factory::ExternalGenericMatrixResource
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
                        }
                        catch (...)
                        {
                            this->deliverable->exception = std::current_exception();
                            this->is_completed_var->exchange(true, std::memory_order_release);
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

                        this->deliverable->exception = nullptr;
                        this->is_completed_var->exchange(true, std::memory_order_release);
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

            auto is_completed() noexcept -> bool
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

    class GetVersionResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/get_version";

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                GetVersionRequest semantic_request      = dg::network_compact_serializer::dgstd_deserialize<GetVersionRequest>(request.payload);
                GetVersionResponse semantic_response    =
                {
                    .response = std::string(MATRIX_OPTIMIZER_SERVER_VERSION_CONTROL),
                    .err_verbal_description = {}
                };

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class OpenClientResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/open_client";

            OpenClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                OpenClientRequest semantic_request      = dg::network_compact_serializer::dgstd_deserialize<OpenClientRequest>(request.payload);
                OpenClientResponse semantic_response;

                try
                {
                    uint64_t client_box_id = this->client_box_manager->open_client_box(semantic_request.connection_config);

                    semantic_response = OpenClientResponse
                    {
                        .result = client_box_id,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    semantic_response = OpenClientResponse
                    {
                        .result = matrix_optimizer_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = matrix_optimizer_server::verbose_error_code(matrix_optimizer_server::to_local_exception_error_code(std::current_exception()))
                    }
                }

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class CloseClientResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/close_client";

            CloseClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                CloseClientRequest semantic_request     = dg::network_compact_serializer::dgstd_deserialize<CloseClientRequest>(request.payload);
                CloseClientResponse semantic_response   = 
                {
                    .result = matrix_optimizer_server::SUCCESS,
                    .err_verbal_description = ""
                };

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class RunResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/run";

            RunResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                RunRequest semantic_request         = dg::network_compact_serializer::dgstd_deserialize<RunRequest>(request.payload);
                RunResponse semantic_response;

                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(semantic_request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    client_box->set_data_source(semantic_request.data_source_config);
                    client_box->set_matrix_resource(semantic_request.matrix_resource);
                    client_box->set_slave_endpoint_vector(semantic_request.url_vec);
                    client_box->set_optimizer(semantic_request.optimizer_config);

                    client_box->run();

                    semantic_response = RunResponse
                    {
                        .result = matrix_optimizer_server::SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    semantic_response = RunResponse
                    {
                        .result = matrix_optimizer_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = matrix_optimizer_server::verbose_error_code(matrix_optimizer_server::to_local_exception_error_code(std::current_exception()))
                    };
                }

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class IsCompletedResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/is_completed";

            IsCompletedResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                IsCompletedRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<IsCompletedRequest>(request.payload);
                IsCompletedResponse semantic_response;

                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(semantic_request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    semantic_response = IsCompletedResponse
                    {
                        .result = client_box->is_completed(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    semantic_response = IsCompletedResponse
                    {
                        .result = std::unexpected(matrix_optimizer_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = matrix_optimizer_server::verbose_error_code(matrix_optimizer_server::to_local_exception_error_code(std::current_exception()))
                    };
                }

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class GetResultResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/get_result";

            GetResultResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                GetResultRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<GetResultRequest>(request.payload);
                GetResultResponse semantic_response;

                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(semantic_request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found{};
                    }

                    if (!client_box->is_completed())
                    {
                        throw optimization_in_progress_error{};
                    }

                    semantic_response
                    {
                        .result = client_box->wait(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    semantic_response = GetResultResponse
                    {
                        .result = std::unexpected(matrix_optimizer_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = matrix_optimizer_server::verbose_error_code(matrix_optimizer_server::to_local_exception_error_code(std::current_exception()))
                    };
                }

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_repsonse),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    void init()
    {
        std::shared_ptr<ClientBoxManager> client_box_manager = std::make_shared<ClientBoxManager>();

        dg_sock::network_rest_frame::server_instance::hook(GetVersionResolver::RESOLVABLE_PATH, std::make_unique<GetVersionResolver>());
        dg_sock::network_rest_frame::server_instance::hook(OpenClientResolver::RESOLVABLE_PATH, std::make_unique<OpenClientResolver>(client_box_manager));
        dg_sock::network_rest_frame::server_instance::hook(CloseClientResolver::RESOLVABLE_PATH, std::make_unique<CloseClientResolver>(client_box_manager));
        dg_sock::network_rest_frame::server_instance::hook(RunResolver::RESOLVABLE_PATH, std::make_unique<RunResolver>(client_box_manager));
        dg_sock::network_rest_frame::server_instance::hook(IsCompletedResolver::RESOLVABLE_PATH, std::make_unique<IsCompletedResolver>(client_box_manager));
        dg_sock::network_rest_frame::server_instance::hook(GetResultResolver::RESOLVABLE_PATH, std::make_unique<GetResultResolver>(client_box_manager));
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