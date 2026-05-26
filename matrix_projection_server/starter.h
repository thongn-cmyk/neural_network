#ifndef __MATRIX_PROJECTION_SERVER_STARTER_H__
#define __MATRIX_PROJECTION_SERVER_STARTER_H__

#include <stdint.h>
#include <stdlib.h>
#include "controller.h"
#include <request_extension/type_based_dgstd_resolutor.h>

namespace matrix_projection_server
{
    void start_server()
    {
        using namespace request_extension::resolutor;

        std::shared_ptr<ClientBoxManager> client_box_manager = std::make_shared<ClientBoxManager>();

        dg_sock::network_rest_frame::server_instance::hook(GetVersionResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>>(std::make_unique<GetVersionResolver>())));
        dg_sock::network_rest_frame::server_instance::hook(OpenClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>>(std::make_unique<OpenClientResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(CloseClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>>(std::make_unique<CloseClientResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(SetMatrixResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<SetMatrixRequest, SetMatrixResponse>>(std::make_unique<SetMatrixResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(ProjectMatrixResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<ProjectMatrixRequest, ProjectMatrixResponse>>(std::make_unique<ProjectMatrixResolver>(client_box_manager))));
    }

    void stop_server() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(ProjectMatrixResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(SetMatrixResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(CloseClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(OpenClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(GetVersionResolver::RESOLVABLE_PATH);
    }
}

#endif