#ifndef __DG_DEVIATION_PROJECTION_SERVER_STARTER_H__
#define __DG_DEVIATION_PROJECTION_SERVER_STARTER_H__

#include "controller.h"
#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <internal_rest/network_rest_frame.h>
#include <string>
#include <request_extension/type_based_dgstd_resolutor.h>
#include <request_extension/type_based_resolutor_interface.h>
#include "local_exception.h"
#include "model.h"

namespace deviation_projection_server
{
    void start_server()
    {
        using namespace request_extension::resolutor;

        std::shared_ptr<ClientBoxManager> client_box_manager = std::make_shared<ClientBoxManager>();

        dg_sock::network_rest_frame::server_instance::hook(GetVersionResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>>(std::make_unique<GetVersionResolver>())));
        dg_sock::network_rest_frame::server_instance::hook(OpenClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>>(std::make_unique<OpenClientResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(CloseClientResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>>(std::make_unique<CloseClientResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(AddTrainingDataResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<AddTrainingDataRequest, AddTrainingDataResponse>>(std::make_unique<AddTrainingDataResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(ClearTrainingDataResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<ClearTrainingDataRequest, ClearTrainingDataResponse>>(std::make_unique<ClearTrainingDataResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(SetMatrixResourceResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<SetMatrixResourceRequest, SetMatrixResourceResponse>>(std::make_unique<SetMatrixResourceResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(GetDeviationResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<GetDeviationRequest, GetDeviationResponse>>(std::make_unique<GetDeviationResolver>(client_box_manager))));
        dg_sock::network_rest_frame::server_instance::hook(SetAndGetDeviationResolutor::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<SetAndGetDeviationRequest, SetAndGetDeviationResponse>>(std::make_unique<SetAndGetDeviationResolutor>(client_box_manager))));
    }

    void stop_server() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(SetAndGetDeviationResolutor::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(GetDeviationResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(SetMatrixResourceResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(ClearTrainingDataResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(AddTrainingDataResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(CloseClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(OpenClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(GetVersionResolver::RESOLVABLE_PATH);
    }
}

#endif