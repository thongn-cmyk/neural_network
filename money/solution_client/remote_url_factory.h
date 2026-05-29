#ifndef __MONEY_SOLUTION_SOLUTION_CLIENT_REMOTE_URL_FACTORY_H__
#define __MONEY_SOLUTION_SOLUTION_CLIENT_REMOTE_URL_FACTORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <string_view>
#include <memory>
#include "model.h"
#include <internal_rest/network_rest_frame.h> //

namespace stock_solution_client
{
    class RemoteUrlFactory
    {
        public:

            static auto get_open_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_server/open_client";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_close_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_server/close_client";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_set_solution_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_server/set_solution";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_get_recommendation_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_server/get_recommendation";

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