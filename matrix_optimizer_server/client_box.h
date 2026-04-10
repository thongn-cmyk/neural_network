#ifndef __MATRIX_OPTIMIZER_CLIENT_BOX_H__
#define __MATRIX_OPTIMIZER_CLIENT_BOX_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <optional>
#include <exception>
#include "local_exception.h"
#include <concurrency_detachable_task/detachable_task_launcher.h>
#include "model.h"
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <data_loader/source_loader/multisource_loader.h>
#include <connection_based_manager/connection_based_manager.h>
// #include <matrix_optimizer_subsystem/matrix_optimizer_subsystem.h>
#include <mutex_extension/fair_mutex.h>
#include <deviation_projector/generic_matrix_wrapper_resource.h>
#include <matrix_optimizer_subsystem/generic_matrix_optimizer.h>
#include <matrix/generic_matrix_factory.h>

namespace matrix_optimizer_server
{
    class ClientBox
    {
        private:

            struct Resource
            {
                std::optional<std::vector<data_loader::source_loader::multisource_loader::MultisourceLoaderConfig>> data_loader_config_vec;
                std::optional<std::vector<dg_sock::network_rest_frame::model::Remote>> remote_vec;

                std::optional<generic_matrix_factory::ExternalGenericMatrixResource> matrix_resource;
                std::optional<deviation_projector::MatrixAsDeviationWrapperConfig> matrix_deviation_wrapper_config;

                std::optional<matrix_optimizer_subsystem::GenericOptimizerConfig> optimizer_config;
            };

            Resource resource;
            std::unique_ptr<concurrency_detachable_task::DetachableTaskHandleInterface<generic_matrix_factory::ExternalGenericMatrixResource>> task;
            bool was_explicitly_destroyed;

        public:

            ClientBox(): resource(),
                         task(nullptr),
                         was_explicitly_destroyed(false){}

            ~ClientBox() noexcept
            {
                this->close(false);
            }

            void set_data_source(const std::vector<data_loader::source_loader::multisource_loader::MultisourceLoaderConfig>& config_vec)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.data_loader_config_vec = config_vec;
            }

            void set_remote_vector(const std::vector<dg_sock::network_rest_frame::model::Remote>& remote_vec)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.remote_vec = remote_vec;
            }

            void set_matrix_resource(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.matrix_resource = matrix_resource;
            }

            void set_matrix_deviation_wrapper(const deviation_projector::MatrixAsDeviationWrapperConfig& config)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                this->resource.matrix_deviation_wrapper_config = config;
            }

            void set_optimizer(const matrix_optimizer_subsystem::GenericOptimizerConfig& config)
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

                if (this->task != nullptr)
                {
                    throw second_run_error{};
                }

                this->task  = concurrency_detachable_task::DetachableTaskLauncher{}.launch(this->make_taskable());
            }

            auto is_completed() -> bool
            {
                if (this->was_explicitly_destroyed)
                {
                    return true;
                }

                if (this->task == nullptr)
                {
                    throw run_not_invoked_error{};
                }

                return this->task->is_completed();
            }

            void interrupt() noexcept
            {
                if (this->was_explicitly_destroyed)
                {
                    return;
                }

                if (this->task == nullptr)
                {
                    return;
                }

                this->task->interrupt();
            }

            auto wait() -> generic_matrix_factory::ExternalGenericMatrixResource
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                if (this->task == nullptr)
                {
                    throw run_not_invoked_error{};
                }

                return this->task->wait();
            }

            void close(bool hard_close = true) noexcept
            {
                if (std::exchange(this->was_explicitly_destroyed, true))
                {
                    return;
                }

                if (this->task == nullptr)
                {
                    return;
                }

                if (hard_close)
                {
                    this->task = nullptr;
                }
                else
                {
                    this->task->detach();
                }
            }

        private:

            auto make_taskable() -> std::shared_ptr<concurrency_task::TaskInterface<generic_matrix_factory::ExternalGenericMatrixResource>>
            {
                return std::make_unique<InternalResolutor>(this->resource);
            }

            class InternalResolutor: public virtual concurrency_task::TaskInterface<generic_matrix_factory::ExternalGenericMatrixResource>
            {
                private:

                    Resource resource;

                public:

                    InternalResolutor(Resource resource_arg)
                    {
                        if (!resource_arg.data_loader_config_vec.has_value())
                        {
                            throw other_invalid_argument("bad datasource config, null config");
                        }

                        if (!resource_arg.remote_vec.has_value())
                        {
                            throw other_invalid_argument("bad url vec, null vec");
                        }

                        if (!resource_arg.matrix_resource.has_value())
                        {
                            throw other_invalid_argument("bad matrix resource config, null config");
                        }

                        if (!resource_arg.matrix_deviation_wrapper_config.has_value())
                        {
                            throw other_invalid_argument("bad matrix deviation wrapper config, null config");
                        }

                        if (!resource_arg.optimizer_config.has_value())
                        {
                            throw other_invalid_argument("bad optimizer config, null config");
                        }

                        this->resource  = std::move(resource_arg);
                    }

                    auto run(common_exception::CancellationTokenInterface& cancellation_token) -> generic_matrix_factory::ExternalGenericMatrixResource
                    {
                        return {};
                        // std::vector<std::unique_ptr<deviation_projection_client::APIClient>> client_vec{};
                        // std::vector<deviation_projection_client::ClientRemote> client_remote_vec{};

                        // for (const auto& remote: this->resource.remote_vec.value())
                        // {
                        //     client_vec.push_back(std::make_unique<deviation_projection_client::APIClient>(remote));
                        //     client_remote_vec.push_back(client_vec.back()->get_client_remote());
                        // }

                        // deviation_projection_ingestion_aid::ClientTrainingDataIngestor{}.set_client_remote(client_remote_vec)
                        //                                                                 .set_data_source(this->resource.data_loader_config_vec.value())
                        //                                                                 .set_cancellation_token(cancellation_token)
                        //                                                                 .run();

                        // std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator  = deviation_projection_matrix_evaluator::DistributedMatrixEvaluatorBuilder{}.set_client_remote(client_remote_vec)
                        //                                                                                                                                                    .set_matrix_deviation_wrapper(this->resource.matrix_deviation_wrapper_config.value())
                        //                                                                                                                                                    .build();

                        // return matrix_optimizer::DistributedOptimizer{}.set_matrix(this->resource.matrix_resource.value())
                        //                                                .set_evaluator(*evaluator)
                        //                                                .set_cancellation_token(cancellation_token)
                        //                                                .set_optimization_config(this->resource.optimizer_config.value())
                        //                                                .optimize();
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

            void set_data_source(const std::vector<data_loader::source_loader::multisource_loader::MultisourceLoaderConfig>& config_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_data_source(config_vec);
            }

            void set_remote_vector(const std::vector<dg_sock::network_rest_frame::model::Remote>& remote_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_remote_vector(remote_vec);
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

            void set_matrix_deviation_wrapper(const deviation_projector::MatrixAsDeviationWrapperConfig& config)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_matrix_deviation_wrapper(config);
            }

            void set_optimizer(const matrix_optimizer_subsystem::GenericOptimizerConfig& config)
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
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                return this->base->is_completed();
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

            auto wait() -> generic_matrix_factory::ExternalGenericMatrixResource
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                return this->base->wait();
            }

            void close(bool hard_close = true) noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->exchange(true, std::memory_order_relaxed))
                {
                    return;
                }

                this->connection->close();
                this->base->close(hard_close);
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