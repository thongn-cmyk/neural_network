#ifndef __MATRIX_OPTIMIZER_SERVER_H__
#define __MATRIX_OPTIMIZER_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix_optimizer_subsystem/generic_matrix_optimizer.h>
#include <main_broker/main_broker.h>
#include <concurrency_base/concurrency_base.h>

namespace matrix_optimizer_server
{
    class ClientBox
    {
        public:

            ClientBox(){}

            void set_data_source(const datasource::Configuration& config)
            {

            }

            void set_matrix_resource(const std::string& resource)
            {

            }

            void set_slave_endpoint_vec(const std::vector<dg_sock::network_rest_frame::model::Url>& url_vec)
            {

            }

            void set_optimizer(const optimizer::GenericOptimizerConfiguration& config)
            {

            }

            void run()
            {

            }

            auto is_completed() noexcept -> bool
            {

            }

            void interrupt() noexcept
            {

            }

            void wait()
            {

            }

            auto get_optimized_matrix_resource() -> generic_matrix_factory::GenericMatrixResource
            {

            }
    };

    class ConnectionBoundClientBox: public virtual connection_based_manager::HealthcheckableInterface
    {
        private:

            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            ClientBox base;
            bool was_explicitly_destroyed;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
        
        public:

            ConnectionBoundClientBox(const connectivity_subsystem::SlaveConfiguration& connection_config): connection(std::make_unique<connectivity_subsystem::SlaveConnection>(connection_config)),
                                                                                                           base(),
                                                                                                           was_explicitly_destroyed(false),
                                                                                                           mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void set_data_source(const datasource::Configuration& config)
            {

            }

            void set_matrix_resource(const std::string& resource)
            {

            }

            void set_slave_endpoint_vec(const std::vector<dg_sock::network_rest_frame::model::Url>& url_vec)
            {

            }

            void set_optimizer(const optimizer::GenericOptimizerConfiguration& config)
            {

            }

            void run()
            {

            }

            auto is_completed() noexcept -> bool
            {

            }

            void interrupt() noexcept
            {

            }

            void wait()
            {

            }

            auto get_optimized_matrix_resource() -> generic_matrix_factory::GenericMatrixResource
            {

            }

            auto is_closed() -> bool
            {

            }

            auto is_alive() -> bool
            {

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

            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {

            }

            void close_client_box(uint64_t client_box_id) noexcept
            {

            }
    };
}

#endif