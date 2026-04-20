#ifndef __CONNECTION_HANDSHAKE_SERVER_H__
#define __CONNECTION_HANDSHAKE_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include "model.h"
#include <request_extension/type_based_dgstd_resolutor.h>
#include <request_extension/type_based_resolutor_interface.h>

namespace connection_handshake_server
{
    template <class T_In, class T_Out>
    using TypeBasedResolutorInterface = request_extension::resolutor::TypeBasedResolutorInterface<T_In, T_Out>;

    class HandshakeResolver: public virtual TypeBasedResolutorInterface<HandshakeRequest, HandshakeResponse>
    {
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "handshake";

            auto handle(const HandshakeRequest& request) -> HandshakeResponse
            {
                return HandshakeResponse
                {
                    .result = "connected"
                };
            }
    };
}

#endif