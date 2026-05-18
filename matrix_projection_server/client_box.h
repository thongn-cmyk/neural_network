#ifndef __MATRIX_PROJECTION_SERVER_CLIENT_BOX_H__
#define __MATRIX_PROJECTION_SERVER_CLIENT_BOX_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <matrix/the_matrix_interface.h>
#include <matrix/matrix_serializer.h>
#include <matrix/generic_matrix_factory.h>
#include <mutex_extension/fair_mutex.h>
#include <atomic>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <connection_based_manager/connection_based_manager.h>

namespace matrix_projection_server
{
    class ClientBox
    {
        private:

            std::unique_ptr<the_matrix::MatrixInterface> matrix;

        public:

            ClientBox(): matrix(nullptr){}

            void set_matrix(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource)
            {
                this->matrix = generic_matrix_factory::GenericMatrixLoader{}.load_resource(generic_matrix_factory::GenericMatrixExternalizer{}.to_internal(matrix_resource));
            }

            auto project(const matrix_serializer::GenericMatrix& generic_in_matrix) -> matrix_serializer::GenericMatrix
            {
                std::shared_ptr<tensor_model::Matrix> in_matrix = matrix_serializer::deserialize(generic_in_matrix);

                return matrix_serializer::serialize(this->matrix->project({in_matrix})[0]);
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


            void set_matrix(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_matrix(matrix_resource);
            }

            auto project(const matrix_serializer::GenericMatrix& generic_in_matrix) -> matrix_serializer::GenericMatrix
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                return this->base->project(generic_in_matrix);
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->exchange(true, std::memory_order_relaxed))
                {
                    return;
                }

                this->connection->close();
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