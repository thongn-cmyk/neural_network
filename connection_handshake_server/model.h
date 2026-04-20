#ifndef __CONNECTION_HANDSHAKE_SERVER_MODEL_H__
#define __CONNECTION_HANDSHAKE_SERVER_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>

namespace connection_handshake_server
{
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