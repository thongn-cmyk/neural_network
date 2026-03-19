#ifndef __MATRIX_OPTIMIZER_SERVER_H__
#define __MATRIX_OPTIMIZER_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix_optimizer_subsystem/generic_matrix_optimizer.h>
#include <main_broker/main_service.h>
#include <concurrency_base/concurrency_base.h>

namespace matrix_optimizer_server
{
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
            std::unique_ptr<main_service::TaskThread> task_thr;
            bool was_run_broke;
            bool was_wait_broke;
            std::shared_ptr<std::atomic<bool>> is_completed_var;
            std::shared_ptr<std::atomic<bool>> interruption_pill;
            std::shared_ptr<Deliverable> deliverable;

        public:

            ClientBox(): resource(),
                         task_thr(),
                         was_run_broke(false),
                         was_wait_broke(false),
                         is_completed_var(std::make_shared<std::atomic<bool>>(false)),
                         interruption_pill(std::make_shared<std::atomic<bool>>(false)),
                         deliverable(std::make_shared<Deliverable>(Deliverable{.matrix_resource = {}, .exception = nullptr})){}

            void set_data_source(const datasource::Configuration& config)
            {
                this->resource.datasource_config = config;
            }

            void set_matrix_resource(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource)
            {
                this->resource.matrix_resource = matrix_resource;
            }

            void set_slave_endpoint_vec(const std::vector<dg_sock::network_rest_frame::model::Url>& url_vec)
            {
                this->resource.url_vec = url_vec;
            }

            void set_optimizer(const optimizer::GenericOptimizerConfiguration& config)
            {
                this->resource.optimizer_config = config;
            }

            void run()
            {
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
                return this->is_completed_var->load(std::memory_order_relaxed);
            }

            void interrupt() noexcept
            {
                this->interruption_pill->exchange(true, std::memory_order_relaxed);
            }

            void wait() -> generic_matrix_factory::ExternalGenericMatrixResource
            {
                if (!this->was_run_broke)
                {
                    throw std::invalid_argument("bad wait, run was not called");
                }

                if (std::exchange(this->was_wait_broke, true))
                {
                    throw std::invalid_argument("bad wait, second wait");
                }

                this->task_thr->join();
                std::rethrow_exception(this->deliverable->exception);

                return this->deliverable->matrix_resource;
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

                    void run(const bool * cancellation_token)
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

                    void internal_run(const bool * cancellation_token)
                    {
                        std::vector<std::unique_ptr<deviation_projection_client::ThreadSafe_APIClient_2>> client_vec{};
                        std::vector<uint64_t> projection_client_id_vec{};

                        for (const auto& url: this->resource.url_vec.value())
                        {
                            client_vec.push_back(std::make_unique<deviation_projection_client::ThreadSafe_APIClient_2>(url));
                            projection_client_id_vec.push_back(client_vec.back()->get_client_id());
                        }

                        data_ingestor::ClientTrainingDataIngestor{}.set_client_id_vec(projection_client_id_vec)
                                                                   .set_data_source(this->resource.datasource_config.value())
                                                                   .run();

                        std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator   = std::make_unique<matrix_evaluator::DistributedMatrixEvaluator>(projection_client_id_vec);
                        common_exception::CancellationTokenFromLambda cancellation_token_obj([&]
                        {
                            return *cancellation_token || this->interruption_pill->load(std::memory_order_relaxed);
                        });

                        this->deliverable->matrix_resource = matrix_optimizer::DistributedOptimizer{}.matrix(this->resource.matrix_resource.value())
                                                                                                     .evaluator(*evaluator)
                                                                                                     .cancellation_token(cancellation_token_obj)
                                                                                                     .optimization_config(this->resource.optimizer_config.value())
                                                                                                     .optimize();

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
                                                                                                           mtx(fair_mutex::make_unique_fair_atomic_flag()),
                                                                                                           connection_mtx(){}

            void set_data_source(const datasource::Configuration& config)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error();
                }

                this->base.set_data_source(config);
            }

            void set_matrix_resource(const std::string& resource)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error();
                }
            }

            void set_slave_endpoint_vec(const std::vector<dg_sock::network_rest_frame::model::Url>& url_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error();
                }
            }

            void set_optimizer(const optimizer::GenericOptimizerConfiguration& config)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error();
                }
            }

            void run()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error();
                }
            }

            auto is_completed() noexcept -> bool
            {
                return this->base.is_completed();
            }

            void interrupt() noexcept
            {
                this->base.interrupt();
            }

            auto wait() -> generic_matrix_factory::ExternalGenericMatrixResource
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->base.wait();
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed.exchange(true, std::memory_order_relaxed))
                {
                    return;
                }

                this->connection->close();
                this->base = nullptr;
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
}

#endif