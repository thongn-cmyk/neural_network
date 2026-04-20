#ifndef __CONNECTION_HANDSHAKE_CLIENT_MODEL_H__
#define __CONNECTION_HANDSHAKE_CLIENT_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <internal_rest/network_rest_frame.h>

namespace connection_handshake_client
{
    template <class T>
    using Promise       = dg_sock::network_rest_frame::client::Promise<T>;

    using Remote        = dg_sock::network_rest_frame::model::Remote;
    using Url           = dg_sock::network_rest_frame::model::Url;
    using ClientRequest = dg_sock::network_rest_frame::model::ClientRequest;

    struct HandshakeRequest
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    struct HandshakeResponse
    {
        std::string result;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result);
        }
    };
}

#endif