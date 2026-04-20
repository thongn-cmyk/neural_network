#ifndef __CONNECTION_HANDSHAKE_CLIENT_REMOTE_URL_FACTORY_H__
#define __CONNECTION_HANDSHAKE_CLIENT_REMOTE_URL_FACTORY_H__

#include <string_view>
#include <internal_rest/network_rest_frame.h>
#include "model.h"

namespace connection_handshake_client
{
    class RemoteUrlFactory
    {
        public:

            static auto get_handshake_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "handshake";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }
    };
}

#endif