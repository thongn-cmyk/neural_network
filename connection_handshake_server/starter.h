#ifndef __CONNECTION_HANDSHAKE_SERVER_STARTER_H__
#define __CONNECTION_HANDSHAKE_SERVER_STARTER_H__

#include <stdint.h>
#include <stdlib.h>
#include "controller.h"

namespace connection_handshake_server
{
    void start_server()
    {
        using namespace request_extension::resolutor;

        dg_sock::network_rest_frame::server_instance::hook(HandshakeResolver::RESOLVABLE_PATH, wrap(std::unique_ptr<TypeBasedResolutorInterface<HandshakeRequest, HandshakeResponse>>(std::make_unique<HandshakeResolver>())));
    }

    void stop_server() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(HandshakeResolver::RESOLVABLE_PATH);
    }
}

#endif