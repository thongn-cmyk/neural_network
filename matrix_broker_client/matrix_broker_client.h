#ifndef __MATRIX_BROKER_CLIENT_H__
#define __MATRIX_BROKER_CLIENT_H__

#include "local_exception.h"
#include "model.h"
#include "remote_url_factory.h"
#include <connection_handshake_client/connection_handshake_client.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <internal_rest/network_rest_frame.h>
#include <serializer/compact_serializer.h>
#include <request_extension/type_based_dgstd_resolutor.h>
#include <string_view>
#include <string>
#include <stl_extension/stdx.h>
#include <exception>
#include <stdexcept>
#include <deviation_projector/generic_matrix_deviation_calculator.h>
#include <expected>

namespace matrix_broker_client
{
    class APIClient
    {
        private:

            dg_sock::network_rest_frame::client::RequestClient client;
            Remote remote;

        public:

            APIClient(const Remote& remote): client(),
                                             remote(remote)
            {
                connection_handshake_client::APIClient(remote).handshake()->wait();
            }

            void set_unique_request(bool is_unique_request)
            {
                this->client.set_unique_request(is_unique_request);
            }

            void set_retry_policy(dg_sock::network_rest_frame::client::retry_policy_t retry_policy)
            {
                this->client.set_retry_policy(retry_policy);
            }

            void set_cancellation_token(const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token)
            {
                this->client.set_cancellation_token(cancellation_token);
            }

            auto broke_matrix(std::string_view generator_id,
                              matrix_entropy_t matrix_entropy,
                              size_t flat_matrix_sz) -> std::shared_ptr<Promise<ClientMatrixResult>>
            {
                using namespace dg_sock::network_rest_frame::client;

                BrokeMatrixRequest raw_request
                {
                    .generator_id   = std::string(generator_id),
                    .matrix_entropy = matrix_entropy,
                    .flat_matrix_sz = static_cast<uint64_t>(flat_matrix_sz)
                };

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_broke_matrix_url(this->remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const BrokeMatrixResponse& response)
                {
                    if (!response.result.has_value())
                    {
                        throw_error_code(response.result.error(), response.err_verbal_description);
                    }

                    return response.result.value();
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<BrokeMatrixResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }
    };
}

#endif