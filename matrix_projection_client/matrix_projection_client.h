#ifndef __MATRIX_PROJECTION_CLIENT_H__
#define __MATRIX_PROJECTION_CLIENT_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <expected>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include "local_exception.h"
#include "model.h"
#include "remote_url_factory.h"
#include <request_extension/type_based_dgstd_resolutor.h>
#include <request_extension/type_based_resolutor_interface.h>
#include <stl_extension/stdx.h>
#include <memory>

namespace matrix_projection_client
{
    class APIClient_Base
    {
        private:

            dg_sock::network_rest_frame::client::RequestClient client;
        
        public:

            void set_unique_request(bool is_unique_request)
            {
                this->client.set_multiple_request_uniqueness(is_unique_request);
            }

            void set_retry_policy(dg_sock::network_rest_frame::client::retry_policy_t retry_policy)
            {
                this->client.set_retry_policy(retry_policy);
            }

            void set_cancellation_token(const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token)
            {
                this->client.set_cancellation_token(cancellation_token);
            }

            auto open_client_box(const Remote& remote,
                                 const connectivity_subsystem::SlaveConfiguration& connection_config) -> std::shared_ptr<Promise<uint64_t>>
            {
                using namespace dg_sock::network_rest_frame::client;

                OpenClientRequest raw_request
                {
                    .connection_config = connection_config
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_open_client_box_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const OpenClientResponse& response)
                {
                    if (!response.result.has_value())
                    {
                        throw_error_code(response.result.error(), response.err_verbal_description);
                    }

                    return response.result.value();
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<OpenClientResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto close_client_box(const Remote& remote, uint64_t client_box_id) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                CloseClientRequest raw_request
                {
                    .client_box_id = client_box_id
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_close_client_box_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const CloseClientResponse& response)
                {
                    throw_error_code(response.result, response.err_verbal_description);

                    return stdx::fancy_void{};
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<CloseClientResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto set_matrix(const Remote& remote, uint64_t client_box_id,
                            const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                SetMatrixRequest raw_request
                {
                    .client_box_id      = client_box_id,
                    .matrix_resource    = matrix_resource
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_set_matrix_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const SetMatrixResponse& response)
                {
                    throw_error_code(response.result, response.err_verbal_description);

                    return stdx::fancy_void{};
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<SetMatrixResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto project_matrix(const Remote& remote, uint64_t client_box_id,
                                const matrix_serializer::GenericMatrix& generic_matrix) -> std::shared_ptr<Promise<matrix_serializer::GenericMatrix>>
            {
                using namespace dg_sock::network_rest_frame::client;

                ProjectMatrixRequest raw_request
                {
                    .client_box_id  = client_box_id,
                    .generic_matrix = generic_matrix
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_project_matrix_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const ProjectMatrixResponse& response)
                {
                    if (!response.result.has_value())
                    {
                        throw_error_code(response.result.error(), response.err_verbal_description);
                    }

                    return response.result.value();
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<ProjectMatrixResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }
    };

    class APIClient
    {
        private:

            APIClient_Base base;
            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            bool was_explicitly_closed;
            ClientRemote client_remote;

        public:

            APIClient(const Remote& remote,
                      std::optional<connectivity_subsystem::MasterConfiguration> config = std::nullopt): base()
            {
                std::unique_ptr<connectivity_subsystem::MasterConnection> connection;

                if (config.has_value())
                {
                    auto tmp_config             = config.value();
                    tmp_config.slave_count      = 1u;
                    connection                  = std::make_unique<connectivity_subsystem::MasterConnection>(tmp_config);
                }
                else
                {
                    connection                  = std::make_unique<connectivity_subsystem::MasterConnection>();
                }

                uint64_t client_id          = this->base.open_client_box(remote, connection->get_slave_configuration())->wait();
                this->connection            = std::move(connection);
                this->was_explicitly_closed = false;

                this->client_remote         =
                {
                    .remote     = remote,
                    .client_id  = client_id
                };
            }

            void set_unique_request(bool is_unique_request)
            {
                this->base.set_unique_request(is_unique_request);
            }

            void set_retry_policy(dg_sock::network_rest_frame::client::retry_policy_t retry_policy)
            {
                this->base.set_retry_policy(retry_policy);
            }

            void set_cancellation_token(const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token)
            {
                this->base.set_cancellation_token(cancellation_token);
            }

            auto set_matrix(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.set_matrix(this->client_remote.remote, this->client_remote.client_id,
                                             matrix_resource);
            }

            auto project_matrix(const matrix_serializer::GenericMatrix& generic_matrix) -> std::shared_ptr<Promise<matrix_serializer::GenericMatrix>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.project_matrix(this->client_remote.remote, this->client_remote.client_id,
                                                 generic_matrix);
            }

            void close(bool hard_close = true) noexcept
            {
                if (std::exchange(this->was_explicitly_closed, true))
                {
                    return;
                }

                try
                {
                    if (hard_close)
                    {
                        this->base.close_client_box(this->client_remote.remote, this->client_remote.client_id)->wait();
                    }
                }
                catch (...)
                {
                    logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("deviation_projection_client")
                                                                                   .topic("APIClient")
                                                                                   .message(std::current_exception()));
                }

                this->connection->close();
            }
        
        private:

            auto can_operate() -> bool
            {
                if (!this->connection->is_alive())
                {
                    return false;
                }

                if (this->was_explicitly_closed)
                {
                    return false;
                }

                return true;
            }
    };
}

#endif