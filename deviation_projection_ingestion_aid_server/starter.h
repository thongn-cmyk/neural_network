#ifndef __DEVIATION_PROJECTION_INGESTION_AID_SERVER_STARTER_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_SERVER_STARTER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <internal_rest/network_rest_frame.h>
#include <request_extension/type_based_dgstd_resolutor.h>
#include "client_box.h"
#include "controller.h"

namespace deviation_projection_ingestion_aid_server
{
    void start_server()
    {
        std::shared_ptr<ClientBoxManager> manager = std::make_shared<ClientBoxManager>();

        {
            using namespace dg_sock::network_rest_frame::server_instance;
            using namespace request_extension::resolutor;

            hook(GetVersionResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>>(std::make_unique<GetVersionResolver>())));
            hook(OpenClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>>(std::make_unique<OpenClientResolver>(manager))));
            hook(CloseClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>>(std::make_unique<CloseClientResolver>(manager))));
            hook(RunResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<RunRequest, RunResponse>>(std::make_unique<RunResolver>(manager))));
            hook(InterruptResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<InterruptRequest, InterruptResponse>>(std::make_unique<InterruptResolver>(manager))));
            hook(IsCompletedResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<IsCompletedRequest, IsCompletedResponse>>(std::make_unique<IsCompletedResolver>(manager))));
            hook(GetResultResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<GetResultRequest, GetResultResponse>>(std::make_unique<GetResultResolver>(manager))));
        }
    }

    void stop_server() noexcept
    {
        using namespace dg_sock::network_rest_frame::server_instance;

        unhook(GetResultResolver::RESOLVABLE_PATH);
        unhook(IsCompletedResolver::RESOLVABLE_PATH);
        unhook(InterruptResolver::RESOLVABLE_PATH);
        unhook(RunResolver::RESOLVABLE_PATH);
        unhook(CloseClientResolver::RESOLVABLE_PATH);
        unhook(OpenClientResolver::RESOLVABLE_PATH);
        unhook(GetVersionResolver::RESOLVABLE_PATH);
    }
}

#endif