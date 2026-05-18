#ifndef __DEVIATION_PROJECTION_CLIENT_REMOTE_URL_FACTORY_H__
#define __DEVIATION_PROJECTION_CLIENT_REMOTE_URL_FACTORY_H__

#include <string_view>
#include <internal_rest/network_rest_frame.h>
#include "model.h"

namespace deviation_projection_client
{
    class RemoteUrlFactory
    {
        public:

            static auto get_open_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "deviation_projection/open_client";
                
                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_close_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "deviation_projection/close_client";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_add_training_data_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "deviation_projection/add_training_data";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_clear_training_data_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "deviation_projection/clear_training_data";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_set_matrix_resource_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "deviation_projection/set_matrix_resource";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_get_deviation_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "deviation_projection/get_deviation";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_set_and_get_deviation_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "deviation_projection/set_and_get_deviation";

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