#ifndef __MATRIX_OPTIMIZER_SERVER_STARTER_H__
#define __MATRIX_OPTIMIZER_SERVER_STARTER_H__

#include <stdint.h>
#include <stdlib.h>
#include "controller.h"
#include <request_extension/type_based_dgstd_resolutor.h>

namespace matrix_optimizer_server
{
    void start_server()
    {
        using namespace request_extension::resolutor;

        std::shared_ptr<ClientBoxManager> client_box_manager = std::make_shared<ClientBoxManager>();

        dg_sock::network_rest_frame::server_instance::hook(GetVersionResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>>(std::make_unique<GetVersionResolver>())));
        dg_sock::network_rest_frame::server_instance::hook(OpenClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>>(std::make_unique<OpenClientResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(CloseClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>>(std::make_unique<CloseClientResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(RunResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<RunRequest, RunResponse>>(std::make_unique<RunResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(InterruptResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<InterruptRequest, InterruptResponse>>(std::make_unique<InterruptResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(IsCompletedResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<IsCompletedRequest, IsCompletedResponse>>(std::make_unique<IsCompletedResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(GetResultResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<GetResultRequest, GetResultResponse>>(std::make_unique<GetResultResolver>(client_box_manager))));
    }

    void stop_server() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(GetResultResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(IsCompletedResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(InterruptResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(RunResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(CloseClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(OpenClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(GetVersionResolver::RESOLVABLE_PATH);
    }
}

#endif