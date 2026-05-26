#ifndef __MONEY_SOLUTION_SOLUTION_SERVER_STARTER_H__
#define __MONEY_SOLUTION_SOLUTION_SERVER_STARTER_H__

#include <stdint.h>
#include <stdlib.h>
#include "controller.h"
#include <request_extension/type_based_dgstd_resolutor.h>
#include "client_box.h"

namespace stock_solution_server
{
    void start_server()
    {
        using namespace request_extension::resolutor;

        std::shared_ptr<ClientBoxManager> manager   = std::make_shared<ClientBoxManager>();

        dg_sock::network_rest_frame::server_instance::hook(GetVersionResolver::RESOLVABLE_PATH, std::unique_ptr<TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>>(std::make_unique<GetVersionResolver>()));
        dg_sock::network_rest_frame::server_instance::hook(OpenClientResolver::RESOLVABLE_PATH, std::unique_ptr<TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>>(std::make_unique<OpenClientResolver>(manager)));
        dg_sock::network_rest_frame::server_instance::hook(CloseClientResolver::RESOLVABLE_PATH, std::unique_ptr<TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>>(std::make_unique<CloseClientResolver>(manager)));
        dg_sock::network_rest_frame::server_instance::hook(SetSolutionResolver::RESOLVABLE_PATH, std::unique_ptr<TypeBasedResolutorInterface<SetSolutionRequest, SetSolutionResponse>>(std::make_unique<SetSolutionResolver>(manager)));
        dg_sock::network_rest_frame::server_instance::hook(GetRecommendationResolver::RESOLVABLE_PATH, std::unique_ptr<TypeBasedResolutorInterface<GetRecommendationRequest, GetRecommendationResponse>>(std::make_unique<GetRecommendationResolver>(manager)));
    }

    void stop_server() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(GetRecommendationResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(SetSolutionResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(CloseClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(OpenClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(GetVersionResolver::RESOLVABLE_PATH);
    }
}

#endif