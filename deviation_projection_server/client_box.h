#ifndef __DG_DEVIATION_PROJECTION_SERVER_CLIENT_BOX_H__
#define __DG_DEVIATION_PROJECTION_SERVER_CLIENT_BOX_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <general_definition/float_def.h>
#include <memory>
#include <internal_rest/network_rest_frame.h>
#include <deviation_projector/generic_matrix_deviation_calculator_interface.h>
#include <deviation_projector/matrix_resource_as_deviation_projector/generic_matrix_resource_as_deviation_projector.h>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <connection_based_manager/connection_based_manager.h>
#include <string>
#include <serializer/compact_serializer.h>
#include <chrono>
#include <request_extension/type_based_dgstd_resolutor.h>
#include <request_extension/type_based_resolutor_interface.h>
#include "local_exception.h"
#include "model.h"
#include <immutable_memory/immutable_memory.h>

namespace deviation_projection_server
{
    class ClientBox
    {
        private:

            std::vector<std::shared_ptr<immutable_memory::ImmutableMemoryInterface>> training_data;
            std::vector<std::unique_ptr<deviation_projector::GenericMatrixDeviationCalculatorInterface>> deviation_calculator_vec;

        public:

            ClientBox(): training_data(),
                         deviation_calculator_vec(){}

            void add_training_data(const std::string& token)
            {
                this->training_data.push_back(std::make_shared<immutable_memory::ManagedImmutableMemory<>>(token));
            }

            void clear_training_data() noexcept
            {
                this->training_data.clear();
            }

            void set_matrix_resource(const std::vector<deviation_projector::matrix_resource_as_deviation_projector::ExternalGenericMatrixResourceAsDeviationCalculatorResource>& matrix_resource_vec)
            {
                this->deviation_calculator_vec.clear();

                for (const auto& e: matrix_resource_vec)
                {
                    this->deviation_calculator_vec.push_back(std::make_unique<deviation_projector::matrix_resource_as_deviation_projector::GenericMatrixResourceAsDeviationCalculator>(e));
                }
            }

            auto get() -> std::vector<mdc_float_t>
            {
                std::vector<mdc_float_t> rs_vec{};

                for (const auto& deviation_calculator: this->deviation_calculator_vec)
                {
                    rs_vec.push_back(deviation_calculator->get_deviation(this->training_data));
                }

                return rs_vec;
            }
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

            void add_training_data(const std::string& token)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base->add_training_data(token);
            }

            void clear_training_data()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base->clear_training_data();
            }

            void set_matrix_resource(const std::vector<deviation_projector::matrix_resource_as_deviation_projector::ExternalGenericMatrixResourceAsDeviationCalculatorResource>& matrix_resource_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base->set_matrix_resource(matrix_resource_vec);
            }

            auto get() -> std::vector<mdc_float_t>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                return this->base->get();
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->exchange(true, std::memory_order_relaxed))
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
                std::shared_ptr<ConnectionBoundClientBox> obj = std::make_shared<ConnectionBoundClientBox>(connection_config);

                return this->base->add(obj);
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