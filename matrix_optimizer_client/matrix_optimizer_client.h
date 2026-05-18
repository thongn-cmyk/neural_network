#ifndef __MATRIX_OPTIMIZER_CLIENT_H__
#define __MATRIX_OPTIMIZER_CLIENT_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "model.h"
#include "local_exception.h"
#include "remote_url_factory.h"
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <vector>
#include <unordered_map>
#include <general_definition/float_def.h>
#include <internal_rest/network_rest_frame.h>
#include <serializer/compact_serializer.h>
#include <request_extension/type_based_dgstd_resolutor.h>
#include <string_view>
#include <string>
#include <stl_extension/stdx.h>
#include <exception>
#include <stdexcept>
#include <expected>
#include <matrix/generic_matrix_factory.h>

namespace matrix_optimizer_client
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

            void set_cancellation_token(const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token)
            {
                this->client.set_cancellation_token(cancellation_token);
            }

            void set_retry_policy(dg_sock::network_rest_frame::client::retry_policy_t retry_policy)
            {
                this->client.set_retry_policy(retry_policy);
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

            auto run(const Remote& remote, uint64_t client_box_id,
                     const RunWorkOrder& run_work_order) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                RunRequest raw_request
                {
                    .client_box_id  = client_box_id,
                    .run_work_order = run_work_order
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_run_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const RunResponse& response)
                {
                    throw_error_code(response.result, response.err_verbal_description);

                    return stdx::fancy_void{};
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<RunResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto interrupt(const Remote& remote, uint64_t client_box_id) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                InterruptRequest raw_request
                {
                    .client_box_id = client_box_id
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_interrupt_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const InterruptResponse& response)
                {
                    throw_error_code(response.result, response.err_verbal_description);

                    return stdx::fancy_void{};
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<InterruptResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto is_completed(const Remote& remote, uint64_t client_box_id) -> std::shared_ptr<Promise<bool>>
            {
                using namespace dg_sock::network_rest_frame::client;

                IsCompletedRequest raw_request
                {
                    .client_box_id = client_box_id
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_is_completed_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const IsCompletedResponse& response)
                {
                    if (!response.result.has_value())
                    {
                        throw_error_code(response.result.error(), response.err_verbal_description);
                    }

                    return response.result.value();
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<IsCompletedResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto get_result(const Remote& remote, uint64_t client_box_id) -> std::shared_ptr<Promise<generic_matrix_factory::ExternalGenericMatrixResource>>
            {
                using namespace dg_sock::network_rest_frame::client;

                GetResultRequest raw_request
                {
                    .client_box_id = client_box_id
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_get_result_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const GetResultResponse& response)
                {
                    if (!response.result.has_value())
                    {
                        throw_error_code(response.result.error(), response.err_verbal_description);
                    }

                    return response.result.value();
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<GetResultResponse>{}, base_resolutor);

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
                      std::optional<connectivity_subsystem::MasterConfiguration> config = std::nulloptr): base(),
                                                                                                          was_explicitly_closed(false)
            {
                std::unique_ptr<connectivity_subsystem::MasterConnection> master_conn;
                
                if (config.has_value())
                {
                    auto tmp_config         = config.value();
                    tmp_config.slave_count  = 1u;
                    master_conn             = std::make_unique<connectivity_subsystem::MasterConnection>(tmp_config);
                }
                else
                {
                    master_conn             = std::make_unique<connectivity_subsystem::MasterConnection>();
                }

                uint64_t client_id  = this->base.open_client_box(remote, master_conn->get_slave_configuration())->wait();
                this->connection    = std::move(master_conn);
                this->client_remote = ClientRemote
                {
                    .remote     = remote,
                    .client_id  = client_id
                };
            }

            ~APIClient() noexcept
            {
                this->close(false);
            }

            void set_unique_request(bool is_unique_request)
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                this->base.set_unique_request(is_unique_request);
            }

            void set_cancellation_token(const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token)
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                this->base.set_cancellation_token(cancellation_token);
            }

            void set_retry_policy(dg_sock::network_rest_frame::client::retry_policy_t retry_policy)
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                this->base.set_retry_policy(retry_policy);
            }

            auto run(const RunWorkOrder& run_work_order) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.run(this->client_remote.remote, this->client_remote.client_id,
                                      run_work_order);
            }

            auto is_completed() -> std::shared_ptr<Promise<bool>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.is_completed(this->client_remote.remote, this->client_remote.client_id);
            }

            auto get_result() -> std::shared_ptr<Promise<generic_matrix_factory::ExternalGenericMatrixResource>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.get_result(this->client_remote.remote, this->client_remote.client_id);
            }

            auto get_remote() const noexcept -> const Remote&
            {
                return this->client_remote.remote;                
            }

            auto get_client_id() const noexcept -> uint64_t
            {
                return this->client_remote.client_id;
            }

            auto get_client_remote() const noexcept -> const ClientRemote&
            {
                return this->client_remote;
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
                    logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("matrix_optimizer_client")
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