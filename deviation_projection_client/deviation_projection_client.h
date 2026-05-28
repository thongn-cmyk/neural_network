#ifndef __DEVIATION_PROJECTION_CLIENT_H__
#define __DEVIATION_PROJECTION_CLIENT_H__

#include <stdint.h>
#include <stdlib.h>
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
// #include <deviation_projector/generic_matrix_deviation_calculator.h>
#include <expected>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include "local_exception.h"
#include "model.h"
#include "remote_url_factory.h"

namespace deviation_projection_client
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

            auto close_client_box(const Remote& remote, uint64_t client_id) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                CloseClientRequest raw_request
                {
                    .client_id = client_id
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

            auto add_training_data(const Remote& remote, uint64_t client_id,
                                   std::string_view training_token) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                AddTrainingDataRequest raw_request
                {
                    .client_id  = client_id,
                    .token      = std::string(training_token),
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_add_training_data_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const AddTrainingDataResponse& response)
                {
                    throw_error_code(response.result, response.err_verbal_description);

                    return stdx::fancy_void{};
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<AddTrainingDataResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto clear_training_data(const Remote& remote, uint64_t client_id) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                ClearTrainingDataRequest raw_request
                {
                    .client_id = client_id
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_clear_training_data_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const ClearTrainingDataResponse& response)
                {
                    throw_error_code(response.result, response.err_verbal_description);

                    return stdx::fancy_void{};
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<ClearTrainingDataResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto set_matrix_resource(const Remote& remote, uint64_t client_id,
                                     const std::vector<ExternalGenericMatrixDeviationCalculatorResource>& matrix_resource_vec) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                SetMatrixResourceRequest raw_request
                {
                    .client_id = client_id,
                    .matrix_resource_vec = matrix_resource_vec
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_set_matrix_resource_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const SetMatrixResourceResponse& response)
                {
                    throw_error_code(response.result, response.err_verbal_description);

                    return stdx::fancy_void{};
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<SetMatrixResourceResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto get_deviation(const Remote& remote, uint64_t client_id) -> std::shared_ptr<Promise<std::vector<mdc_float_t>>>
            {
                using namespace dg_sock::network_rest_frame::client;

                GetDeviationRequest raw_request
                {
                    .client_id = client_id
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_get_deviation_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const GetDeviationResponse& response)
                {
                    if (!response.result.has_value())
                    {
                        throw_error_code(response.result.error(), response.err_verbal_description);
                    }

                    return response.result.value();
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<GetDeviationResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            auto set_and_get_deviation(const Remote& remote, uint64_t client_id,
                                       const std::vector<ExternalGenericMatrixDeviationCalculatorResource>& matrix_resource_vec) -> std::shared_ptr<Promise<std::vector<mdc_float_t>>>
            {
                using namespace dg_sock::network_rest_frame::client;

                SetAndGetDeviationRequest raw_request
                {
                    .client_id = client_id,
                    .matrix_resource_vec = matrix_resource_vec
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_set_and_get_deviation_url(remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const SetAndGetDeviationResponse& response)
                {
                    if (!response.result.has_value())
                    {
                        throw_error_code(response.result.error(), response.err_verbal_description);
                    }

                    return response.result.value();
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<SetAndGetDeviationResponse>{}, base_resolutor);

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

            ~APIClient() noexcept
            {
                this->close(false);
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

            auto add_training_data(std::string_view token) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.add_training_data(this->client_remote.remote, this->client_remote.client_id, token);
            }

            auto clear_training_data() -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.clear_training_data(this->client_remote.remote, this->client_remote.client_id);
            }

            auto set_matrix_resource(const std::vector<ExternalGenericMatrixDeviationCalculatorResource>& matrix_resource_vec) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.set_matrix_resource(this->client_remote.remote, this->client_remote.client_id, matrix_resource_vec);
            }

            auto get_deviation() -> std::shared_ptr<Promise<std::vector<mdc_float_t>>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.get_deviation(this->client_remote.remote, this->client_remote.client_id);
            }

            auto set_and_get_deviation(const std::vector<ExternalGenericMatrixDeviationCalculatorResource>& matrix_resource_vec) -> std::shared_ptr<Promise<std::vector<mdc_float_t>>>
            {
                if (!this->can_operate())
                {
                    throw inoperable_client_error{};
                }

                return this->base.set_and_get_deviation(this->client_remote.remote, this->client_remote.client_id, matrix_resource_vec);
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

    class NoOwned_APIClient
    {
        private:

            APIClient_Base base;
            ClientRemote client_remote;

        public:

            NoOwned_APIClient(const Remote& remote,
                              uint64_t client_id): base(),
                                                   client_remote({.remote = remote, .client_id = client_id}){}

            NoOwned_APIClient(const ClientRemote& client_remote): base(),
                                                                  client_remote(client_remote){}

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

            auto add_training_data(std::string_view token) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                return this->base.add_training_data(this->client_remote.remote, this->client_remote.client_id, token);
            }

            auto clear_training_data() -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                return this->base.clear_training_data(this->client_remote.remote, this->client_remote.client_id);
            }

            auto set_matrix_resource(const std::vector<ExternalGenericMatrixDeviationCalculatorResource>& matrix_resource_vec) -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                return this->base.set_matrix_resource(this->client_remote.remote, this->client_remote.client_id, matrix_resource_vec);
            }

            auto get_deviation() -> std::shared_ptr<Promise<std::vector<mdc_float_t>>>
            {
                return this->base.get_deviation(this->client_remote.remote, this->client_remote.client_id);
            }

            auto set_and_get_deviation(const std::vector<ExternalGenericMatrixDeviationCalculatorResource>& matrix_resource_vec) -> std::shared_ptr<Promise<std::vector<mdc_float_t>>>
            {
                return this->base.set_and_get_deviation(this->client_remote.remote, this->client_remote.client_id, matrix_resource_vec);
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
    };
}

#endif