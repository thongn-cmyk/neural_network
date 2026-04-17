#ifndef __MATRIX_PROJECTION_CLIENT_REMOTE_URL_FACTORY_H__
#define __MATRIX_PROJECTION_CLIENT_REMOTE_URL_FACTORY_H__

#include <string_view>
#include "model.h"

namespace matrix_projection_client
{
    class RemoteUrlFactory
    {
        public:

            static auto get_open_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_projection_server/open_client";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_close_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_projection_server/close_client";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_set_matrix_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_projection_server/set_matrix";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_project_matrix_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_projection_server/project_matrix";

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