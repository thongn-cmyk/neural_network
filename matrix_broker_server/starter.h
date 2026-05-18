#ifndef __MATRIX_BROKER_SERVER_STARTER_H__
#define __MATRIX_BROKER_SERVER_STARTER_H__

#include "controller.h"
#include "service.h"
#include "global_matrix_broker.h"
#include <request_extension/type_based_dgstd_resolutor.h>
#include <internal_rest/network_rest_frame.h>
#include <memory>

namespace matrix_broker_server
{
    void start_server()
    {
        using namespace request_extension::resolutor;

        matrix_broker_server::global_matrix_broker::init();

        std::shared_ptr<MatrixBrokerageServiceInterface> service = std::make_shared<MatrixBrokerageService>();
        dg_sock::network_rest_frame::server_instance::hook(BrokeMatrixResolutor::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<BrokeMatrixRequest, BrokeMatrixResponse>>(std::make_unique<BrokeMatrixResolutor>(service))));
    }

    void stop_server() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(BrokeMatrixResolutor::RESOLVABLE_PATH);
    }
}

#endif