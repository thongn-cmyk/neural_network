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
#include <deviation_projection_client/deviation_projection_client.h>
#include <deviation_projection_ingestion_aid/deviation_projection_ingestion_aid.h>

namespace matrix_optimizer_server
{
    class ClientBox
    {
        private:

            std::unique_ptr<concurrency_detachable_task::DetachableTaskHandleInterface<generic_matrix_factory::ExternalGenericMatrixResource>> task;
            bool was_explicitly_destroyed;

        public:

            ClientBox(): task(nullptr),
                         was_explicitly_destroyed(false){}

            ~ClientBox() noexcept
            {
                this->close(false);
            }

            void run(const RunWorkOrder& run_work_order)
            {
                if (this->was_explicitly_destroyed)
                {
                    throw destroyed_client_box_error{};
                }

                if (this->task != nullptr)
                {
                    throw second_run_error{};
                }

                this->task  = concurrency_detachable_task::DetachableTaskLauncher{}.launch(this->make_taskable(run_work_order));
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
                    this->task->interrupt();
                    this->task->detach();
                }
            }

        private:

            auto make_taskable(const RunWorkOrder& run_work_order) -> std::unique_ptr<concurrency_task::TaskInterface<generic_matrix_factory::ExternalGenericMatrixResource>>
            {
                return std::make_unique<InternalResolutor>(run_work_order);
            }

            class CancellationTokenWrapper: public virtual common_exception::CancellationTokenInterface
            {
                private:

                    common_exception::CancellationTokenInterface * base;
                
                public:

                    CancellationTokenWrapper(common_exception::CancellationTokenInterface * base): base(stdx::safe_ptr_access(base)){}

                    auto is_canceled() noexcept -> bool
                    {
                        return this->base->is_canceled();
                    }
            };

            class InternalResolutor: public virtual concurrency_task::TaskInterface<generic_matrix_factory::ExternalGenericMatrixResource>
            {
                private:

                    RunWorkOrder work_order;

                public:

                    InternalResolutor(const RunWorkOrder& work_order): work_order(work_order){}

                    auto run(common_exception::CancellationTokenInterface& cancellation_token) -> generic_matrix_factory::ExternalGenericMatrixResource
                    {
                        std::vector<std::unique_ptr<deviation_projection_client::APIClient>> client_vec{};

                        for (const auto& remote: this->get_remote_vec())
                        {
                            client_vec.push_back(std::make_unique<deviation_projection_client::APIClient>(remote));
                        }

                        deviation_projection_ingestion_aid::ClientTrainingDataPiecewiseIngestor ingestor{};

                        for (size_t i = 0u; i < this->work_order.pull_work_order_vec.size(); ++i)
                        {
                            ingestor.add(deviation_projection_ingestion_aid::PiecewiseBuilder{}.worker_remote(this->work_order.pull_work_order_vec[i].worker_remote)
                                                                                               .client_remote(client_vec[i]->get_remote(), client_vec[i]->get_client_id())
                                                                                               .data_loader_config(this->work_order.pull_work_order_vec[i].data_loader_config)
                                                                                               .firer_config(this->work_order.pull_work_order_vec[i].firer_config)
                                                                                               .build());
                        }

                        ingestor.run(cancellation_token);

                        std::shared_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator  = deviation_projection_matrix_evaluator::MatrixEvaluatorBuilder{}.set_client_remote(client_remote_vec)
                                                                                                                                                                .set_exportable_matrix(this->work_order.matrix)
                                                                                                                                                                .set_matrix_deviation_wrapper(this->work_order.deviation_config)
                                                                                                                                                                .set_cancellation_token(std::make_shared<CancellationTokenWrapper>(&cancellation_token))
                                                                                                                                                                .build();

                        return matrix_optimizer::Optimizer{}.set_matrix(this->work_order.matrix)
                                                            .set_evaluator(evaluator)
                                                            .set_optimization_config(this->work_order.optimizer_config)
                                                            .optimize(cancellation_token);
                    }
                
                private:

                    auto get_remote_vec() -> std::vector<Remote>
                    {
                        std::vector<Remote> rs{};

                        for (const auto& pull_wo: this->work_order.pull_work_order_vec)
                        {
                            rs.push_back(pull_wo.worker_remote);
                        }

                        return rs;
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

            ~ConnectionBoundClientBox() noexcept
            {
                this->close(false);
            }

            void run(const RunWorkOrder& work_order)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->run(work_order);
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