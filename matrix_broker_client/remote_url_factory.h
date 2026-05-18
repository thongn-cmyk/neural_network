#ifndef __MATRIX_BROKER_CLIENT_REMOTE_URL_FACTORY_H__
#define __MATRIX_BROKER_CLIENT_REMOTE_URL_FACTORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <string_view>

namespace matrix_broker_client
{
    class RemoteUrlFactory
    {
        public:

            static auto get_broke_matrix_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_broker/broke_matrix";

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