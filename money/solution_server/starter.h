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
        using namespace dg_sock::network_rest_frame::server_instance;

        std::shared_ptr<ClientBoxManager> manager   = std::make_shared<ClientBoxManager>();

        hook(GetVersionResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>>(std::make_unique<GetVersionResolver>())));
        hook(OpenClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>>(std::make_unique<OpenClientResolver>(manager))));
        hook(CloseClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>>(std::make_unique<CloseClientResolver>(manager))));
        hook(SetSolutionResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<SetSolutionRequest, SetSolutionResponse>>(std::make_unique<SetSolutionResolver>(manager))));
        hook(GetRecommendationResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<GetRecommendationRequest, GetRecommendationResponse>>(std::make_unique<GetRecommendationResolver>(manager))));
    }

    void stop_server() noexcept
    {
        using namespace dg_sock::network_rest_frame::server_instance;

        unhook(GetRecommendationResolver::RESOLVABLE_PATH);
        unhook(SetSolutionResolver::RESOLVABLE_PATH);
        unhook(CloseClientResolver::RESOLVABLE_PATH);
        unhook(OpenClientResolver::RESOLVABLE_PATH);
        unhook(GetVersionResolver::RESOLVABLE_PATH);
    }
}

#endif