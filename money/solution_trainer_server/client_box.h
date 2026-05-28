#ifndef __MONEY_SOLUTION_SOLUTION_TRAINER_SERVER_CLIENT_BOX_H__
#define __MONEY_SOLUTION_SOLUTION_TRAINER_SERVER_CLIENT_BOX_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "model.h"
#include "local_exception.h"
#include <mutex_extension/fair_mutex.h>
#include <atomic>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <connection_based_manager/connection_based_manager.h>
#include <stl_extension/semantic_mapper.h>
#include <client_box/task_box/task_box.h>

//we'd dedicate a few days to fix these inventions

namespace stock_solution_trainer_server
{
    template <class T>
    using Task = concurrency_task::TaskInterface<T>;

    class ClientBox: public client_box::task_box::ClientTaskBox<RunWorkOrder, ExternalSolutionData>
    {
        protected:

            auto make_task(const RunWorkOrder& work_order) -> std::unique_ptr<Task<ExternalSolutionData>>
            {
                return std::make_unique<InternalResolutor>(work_order);
            }

        private:

            class InternalResolutor: public virtual Task<ExternalSolutionData>
            {
                private:

                    RunWorkOrder work_order;
                
                public:

                    InternalResolutor(const RunWorkOrder& work_order): work_order(work_order){}

                    auto run(common_exception::CancellationTokenInterface& cancellation_token) -> ExternalSolutionData
                    {
                        common_exception::ObjectLifeCancellationTokenStackHolder cancellation_token_holder(cancellation_token);

                        auto solution_builder   = SolutionBuilder{}.set_cancellation_token(cancellation_token_holder.get())
                                                                   .set_data_loader_config(this->work_order.data_source.data_loader_config)
                                                                   .set_from(this->work_order.training_window.from_timepoint)
                                                                   .set_to(this->work_order.training_window.to_timepoint)
                                                                   .set_iteration_step(this->work_order.training_window.iteration_step)
                                                                   .set_optimization_flag(this->work_order.optimization_flag);

                        for (const auto& sink: this->work_order.compute_sink_vec)
                        {
                            solution_builder.add_compute_sink(sink.sink_remote, sink.firer_config);
                        }
                    
                        return solution_builder.build();
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

            ConnectionBoundClientBox(const connectivity_subsystem::SlaveConfiguration& connection_config): connection(std::make_unique<>()),
                                                                                                           base(std::make_unique<ClientBox>()),
                                                                                                           was_explicitly_destroyed(std::make_unique<std::atomic<bool>>(false)),
                                                                                                           mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void run(const RunWorkOrder& work_order)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                this->base->run(work_order);
            }

            auto is_completed() -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->base->is_completed();
            }

            void interrupt() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                this->base->interrupt();
            }

            auto wait() -> ExternalSolutionData
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

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
                std::shared_ptr<ConnectionBoundClientBox> client_box = this->get_client_box(client_box_id);

                if (client_box != nullptr)
                {
                    client_box->close();
                }

                this->base->close(client_box_id);
            }
    };
}

#endif

