#ifndef __MONEY_SOLUTION_SOLUTION_TRAINER_CLIENT_REMOTE_URL_FACTORY_H__
#define __MONEY_SOLUTION_SOLUTION_TRAINER_CLIENT_REMOTE_URL_FACTORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <string_view>
#include <memory>

namespace solution_trainer_client
{
    class RemoteUrlFactory
    {
        public:
            
            static auto get_open_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_trainer_server/open_client";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };                
            }

            static auto get_close_client_box_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_trainer_server/close_client";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_run_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_trainer_server/run";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_interrupt_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_trainer_server/interrupt";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_is_completed_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_trainer_server/is_completed";

                return Url
                {
                    .remote_addr    = remote.addr,
                    .resource_addr  = dg_sock::string(resource_addr),
                    .channel        = remote.channel
                };
            }

            static auto get_get_result_url(const Remote& remote) -> Url
            {
                std::string_view resource_addr  = "stock_solution_trainer_server/get_result";

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