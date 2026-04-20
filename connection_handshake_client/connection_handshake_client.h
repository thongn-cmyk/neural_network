#ifndef __CONNECTION_HANDSHAKE_CLIENT_H__
#define __CONNECTION_HANDSHAKE_CLIENT_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <expected>
#include "model.h"
#include "remote_url_factory.h"
#include <request_extension/type_based_dgstd_resolutor.h>
#include <request_extension/type_based_resolutor_interface.h>
#include <stl_extension/stdx.h>
#include <memory>
#include "local_exception.h"

namespace connection_handshake_client
{
    class APIClient
    {
        private:

            dg_sock::network_rest_frame::client::RequestClient client;
            Remote remote;

        public:

            APIClient(const Remote& remote): client(),
                                             remote(remote){}

            auto handshake() -> std::shared_ptr<Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                HandshakeRequest raw_request{};

                std::string request_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(raw_request);
                ClientRequest request       = RequestFactory{}.url(RemoteUrlFactory::get_handshake_url(this->remote))
                                                              .payload(request_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto base_resolutor = [](const HandshakeResponse& response)
                {
                    if (response.result != "connected")
                    {
                        throw bad_connect_error{};
                    }

                    return stdx::fancy_void{};
                };

                request_extension::resolutor::ClientResponseDgstdFormatter resolutor(stdx::Tag<HandshakeResponse>{}, base_resolutor);

                return this->client.request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }
    };
}

#endif