#ifndef __MATRIX_OPTIMIZER_CLIENT_REMOTE_URL_FACTORY_H__
#define __MATRIX_OPTIMIZER_CLIENT_REMOTE_URL_FACTORY_H__

#include <string_view>
#include <internal_rest/network_rest_frame.h>
#include "model.h"

namespace matrix_optimizer_client
{
    class RemoteUrlFactory
    {
        public:

            static auto get_open_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_optimizer/open_client";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_close_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_optimizer/close_client";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_run_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_optimizer/run";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_interrupt_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_optimizer/is_completed";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_is_completed_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_optimizer/interrupt";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_get_result_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr = "matrix_optimizer/get_result";

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